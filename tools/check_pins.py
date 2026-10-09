#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Vérifie que chaque dépendance du build et de la CI est épinglée par révision complète.

Règles (docs/development/build.md, section « Dépendances épinglées ») :

- action GitHub : commit complet (40 caractères hexadécimaux) ;
- image de conteneur : empreinte SHA-256 ;
- paquets apt : uniquement par tools/ci/apt-install.sh, qui installe depuis un instantané daté ;
- paquets Python : uniquement par pip avec --require-hashes et un fichier de tools/requirements/,
  dont chaque entrée est en version exacte avec au moins une empreinte SHA-256 ;
- téléchargement (curl, wget) : contrôlé par sha256sum dans le même pas ;
- version de Python installée par setup-python : version complète X.Y.Z ;
- dépendance CMake (FetchContent, ExternalProject) : GIT_TAG de commit complet ou URL_HASH SHA256.

Le contrôle est textuel : il ne remplace pas la relecture d'une montée de version.

Usage : check_pins.py [racine_du_dépôt]
Code de retour : 0 si toutes les dépendances sont épinglées, 1 sinon.
"""

import re
import sys
from pathlib import Path

DEFAULT_ROOT = Path(__file__).resolve().parent.parent

USES = re.compile(r"^\s*(?:-\s+)?uses:\s*['\"]?([^\s'\"#]+)")
IMAGE = re.compile(r"^\s*(?:-\s+)?(?:image|container):\s*['\"]?(\$\{\{[^}]*\}\}|[^\s'\"#]+)")
MATRIX_REFERENCE = re.compile(r"^\$\{\{\s*matrix((?:\.[A-Za-z_][A-Za-z0-9_-]*)+)\s*\}\}$")
RUN = re.compile(r"^(\s*)(-\s+)?run:\s*(.*)$")
PYTHON_VERSION = re.compile(r"^\s*python-version:\s*['\"]?(\$\{\{[^}]*\}\}|[^\s'\"#]+)")
ENV_VALUE = re.compile(r"^\s+([A-Z][A-Z0-9_]*):\s*['\"]?([^\s'\"#]+)")
ENV_REFERENCE = re.compile(r"^\$\{\{\s*env\.([A-Z][A-Z0-9_]*)\s*\}\}$")
COMMIT = re.compile(r"@[0-9a-f]{40}$")
DIGEST = re.compile(r"@sha256:[0-9a-f]{64}$")
FULL_VERSION = re.compile(r"^\d+\.\d+\.\d+$")

APT = re.compile(r"\bapt(?:-get)?\s+(?:-\S+\s+)*(?:install|update|upgrade)\b")
# « pip install », y compris par un chemin entre guillemets (« "$VENV/bin/pip" install ») ou « -m pip ».
PIP_INSTALL = re.compile(r"\bpip3?['\"]?\s+install\b")
DOWNLOAD = re.compile(r"\b(?:curl|wget)\b")
CHECKSUM = re.compile(r"\bsha256sum\s+(?:--check|-c)\b")
UNCONTROLLED = re.compile(r"\b(?:pipx|npx|npm\s+(?:install|i|ci)|gem\s+install|go\s+install|cargo\s+install|uvx)\b")

REQUIREMENT = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*(?:\[[^\]]*\])?==[^\s;]+")
CMAKE_DEPENDENCY = re.compile(r"\b(FetchContent_Declare|ExternalProject_Add)\s*\(([^)]*)\)", re.IGNORECASE | re.DOTALL)
GIT_TAG = re.compile(r"\bGIT_TAG\s+(\S+)")
URL_HASH = re.compile(r"\bURL_HASH\s+SHA256=[0-9a-fA-F]{64}\b")


def run_blocks(lines: list[str]) -> list[tuple[int, str]]:
    """Commandes des clés « run » d'un workflow : (numéro de ligne, texte du bloc)."""
    blocks = []
    index = 0
    while index < len(lines):
        match = RUN.match(lines[index])
        index += 1
        if not match:
            continue
        indent = len(match.group(1)) + len(match.group(2) or "")
        value = match.group(3).strip()
        start = index
        if value and value[0] not in "|>":
            blocks.append((start, value))
            continue
        body = []
        while index < len(lines) and (not lines[index].strip() or
                                      len(lines[index]) - len(lines[index].lstrip()) > indent):
            body.append(lines[index])
            index += 1
        blocks.append((start + 1, "\n".join(body)))
    return blocks


def matrix_values(lines: list[str], key: str) -> list[tuple[int, str]]:
    """Valeurs données à la clé « key » dans le workflow : scalaire, liste en ligne,
    liste en bloc ou table en ligne (« { name: a, key: b } ») ; (numéro de ligne, valeur)."""
    values = []
    block_key = re.compile(rf"^(\s*)(?:-\s+)?{re.escape(key)}:\s*(.*)$")
    flow_key = re.compile(rf"[{{,]\s*{re.escape(key)}:\s*([^,}}]+)")
    for index, line in enumerate(lines):
        number = index + 1
        for match in flow_key.finditer(line):
            values.append((number, match.group(1).strip().strip("'\"")))
        match = block_key.match(line)
        if not match:
            continue
        rest = match.group(2).split(" #", 1)[0].strip()
        if rest.startswith("["):
            values += [(number, item.strip().strip("'\"")) for item in rest.strip("[]").split(",") if item.strip()]
        elif rest:
            values.append((number, rest.strip("'\"")))
        else:
            indent = len(match.group(1))
            following = index + 1
            while following < len(lines) and (not lines[following].strip() or
                                              len(lines[following]) - len(lines[following].lstrip()) > indent):
                item = lines[following].strip()
                if item.startswith("- "):
                    values.append((following + 1, item[2:].strip().strip("'\"")))
                following += 1
    # La ligne qui référence la matrice (« image: ${{ matrix.image }} ») n'en est pas une valeur.
    return [(number, value) for number, value in values if not value.startswith("${{")]


def check_workflow(path: Path, label: str) -> list[str]:
    errors = []
    lines = path.read_text(encoding="utf-8").splitlines()
    # Variables d'environnement du workflow, pour résoudre « ${{ env.NOM }} ».
    env = {m.group(1): m.group(2) for line in lines if (m := ENV_VALUE.match(line))}

    def resolve(value: str) -> str:
        reference = ENV_REFERENCE.match(value)
        return env.get(reference.group(1), value) if reference else value

    for number, line in enumerate(lines, 1):
        where = f"{label}:{number}"
        if match := USES.match(line):
            ref = match.group(1)
            if ref.startswith("./"):
                pass
            elif ref.startswith("docker://"):
                if not DIGEST.search(ref):
                    errors.append(f"{where}: action « {ref} » sans empreinte SHA-256")
            elif not COMMIT.search(ref):
                errors.append(f"{where}: action « {ref} » non épinglée par commit complet")
        if match := IMAGE.match(line):
            image = resolve(match.group(1))
            if not image.startswith("${{"):
                if not DIGEST.search(image):
                    errors.append(f"{where}: image « {image} » sans empreinte SHA-256")
            elif reference := MATRIX_REFERENCE.match(image):
                # Image tirée d'une matrice : chaque valeur de la clé référencée est contrôlée,
                # quel que soit son nom.
                key = reference.group(1).rsplit(".", 1)[1]
                values = matrix_values(lines, key)
                if not values:
                    errors.append(f"{where}: valeurs de « {image} » introuvables dans la matrice")
                for value_line, value in values:
                    # Une valeur sous une clé « image: » est déjà contrôlée comme image littérale.
                    if IMAGE.match(lines[value_line - 1]):
                        continue
                    if not DIGEST.search(value):
                        errors.append(f"{label}:{value_line}: image « {value} » (matrice, {image}) "
                                      "sans empreinte SHA-256")
            else:
                errors.append(f"{where}: image « {image} » : expression non vérifiable")
        if (match := PYTHON_VERSION.match(line)) and not FULL_VERSION.match(version := resolve(match.group(1))):
            errors.append(f"{where}: python-version « {version} » n'est pas une version complète X.Y.Z")
    for number, block in run_blocks(lines):
        where = f"{label}:{number}"
        if APT.search(block):
            errors.append(f"{where}: appel direct à apt ; utiliser tools/ci/apt-install.sh")
        if match := UNCONTROLLED.search(block):
            errors.append(f"{where}: « {match.group(0)} » installe sans épinglage contrôlé")
        for line in block.splitlines():
            if PIP_INSTALL.search(line) and not ("--require-hashes" in line and "tools/requirements/" in line):
                errors.append(f"{where}: pip install sans --require-hashes et fichier de tools/requirements/")
        if DOWNLOAD.search(block) and not CHECKSUM.search(block):
            errors.append(f"{where}: téléchargement sans contrôle sha256sum dans le même pas")
    return errors


def check_requirements(path: Path, label: str) -> list[str]:
    """Chaque entrée (lignes de continuation jointes) : nom==version et au moins une empreinte."""
    errors, entries, current, start = [], [], [], 0
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        content = line.strip()
        if not current and (not content or content.startswith("#")):
            continue
        if not current:
            start = number
        current.append(content.removesuffix("\\").strip())
        if not content.endswith("\\"):
            entries.append((start, " ".join(current)))
            current = []
    if current:
        entries.append((start, " ".join(current)))
    if not entries:
        errors.append(f"{label}: aucune entrée")
    for number, entry in entries:
        where = f"{label}:{number}"
        name = entry.split()[0]
        if not REQUIREMENT.match(entry):
            errors.append(f"{where}: « {name} » n'est pas en version exacte (nom==version)")
        elif "--hash=sha256:" not in entry:
            errors.append(f"{where}: « {name} » sans empreinte --hash=sha256")
    return errors


def check_cmake(path: Path, label: str) -> list[str]:
    errors = []
    text = path.read_text(encoding="utf-8")
    for match in CMAKE_DEPENDENCY.finditer(text):
        where = f"{label}:{text.count(chr(10), 0, match.start()) + 1}"
        body = match.group(2)
        tag = GIT_TAG.search(body)
        if tag and not re.fullmatch(r"[0-9a-f]{40}", tag.group(1)):
            errors.append(f"{where}: {match.group(1)} avec GIT_TAG « {tag.group(1)} », pas un commit complet")
        elif not tag and not URL_HASH.search(body):
            errors.append(f"{where}: {match.group(1)} sans GIT_TAG de commit complet ni URL_HASH SHA256")
    return errors


def check(root: Path) -> list[str]:
    errors = []
    for path in sorted((root / ".github" / "workflows").glob("*.y*ml")):
        errors += check_workflow(path, str(path.relative_to(root)))
    for path in sorted((root / "tools" / "requirements").glob("*.txt")):
        errors += check_requirements(path, str(path.relative_to(root)))
    cmake_files = [p for p in root.rglob("*") if p.is_file() and (p.name == "CMakeLists.txt" or p.suffix == ".cmake")]
    for path in sorted(cmake_files):
        relative = path.relative_to(root)
        if relative.parts[0] in {"build", ".git"}:
            continue
        errors += check_cmake(path, str(relative))
    return errors


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else DEFAULT_ROOT
    if not (root / ".github" / "workflows").is_dir():
        print(f"Workflows introuvables sous : {root}", file=sys.stderr)
        return 1
    errors = check(root)
    for error in errors:
        print(f"erreur: {error}", file=sys.stderr)
    print("Dépendances " + ("non épinglées." if errors else "épinglées."))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
