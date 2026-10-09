#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Vérifie la cohérence du registre de traçabilité de Clepsydre.

Le registre (software_development_file/registry/) relie exigences, risques,
mesures de maîtrise, vérifications et composants tiers (SOUP). Le script
signale les identifiants mal formés ou en double, les références vers des
éléments inexistants (y compris les identifiants cités dans la source d'une
exigence et le fichier de preuve d'une vérification), les éléments orphelins,
les chaînes de traçabilité incomplètes et les exigences système ou matérielles
sans vérification sur banc.

Usage : check_registry.py [répertoire_du_registre]
Code de retour : 0 si le registre est cohérent, 1 sinon.
"""

import re
import sys
import tomllib
from pathlib import Path

DEFAULT_REGISTRY = Path(__file__).resolve().parent.parent / "software_development_file" / "registry"

# Pour chaque fichier : clé des entrées, motif d'identifiant, champs obligatoires.
# Un champ texte est une chaîne non vide ; un champ liste est une liste non vide de chaînes.
TEXT, LIST = "texte", "liste"
SCHEMA = {
    "requirements.toml": ("requirement", r"(SYS|SW|HW)-REQ-\d{3}",
                          {"title": TEXT, "text": TEXT, "source": TEXT, "status": TEXT}),
    "risks.toml": ("risk", r"RISK-\d{3}", {"hazard": TEXT, "harm": TEXT, "status": TEXT}),
    "controls.toml": ("control", r"CTRL-\d{3}", {"description": TEXT, "risks": LIST, "requirements": LIST}),
    "verifications.toml": ("verification", r"VER-\d{3}", {"method": TEXT, "level": TEXT, "requirements": LIST,
                                                          "status": TEXT}),
    "soup.toml": ("soup", r"SOUP-\d{3}", {"name": TEXT, "version": TEXT, "supplier": TEXT, "license": TEXT,
                                          "usage": TEXT, "requirements": LIST, "known_anomalies": TEXT}),
}

# Champs facultatifs admis en plus de « id » et des champs obligatoires.
OPTIONAL = {"verification": {"reference": TEXT}, "soup": {"purl": TEXT}}

STATUSES = {
    "requirement": {"draft", "approved", "obsolete"},
    "risk": {"draft", "controlled", "accepted"},
    "verification": {"planned", "passed", "failed"},
}

METHODS = {"test", "analysis", "inspection", "demonstration"}

# Niveaux de vérification : software_development_file/verification-strategy.md.
LEVELS = {"unit", "integration", "system", "bench", "precompliance"}

# Niveaux sur la cible matérielle : une exigence système ou matérielle doit y être vérifiée,
# le simulateur ne reproduisant que ce que son modèle contient.
BENCH_LEVELS = {"bench", "precompliance"}

# Identifiants du registre cités dans le texte libre d'une source d'exigence.
CITED_ID = re.compile(r"\b(?:(?:SYS|SW|HW)-REQ|RISK|CTRL|VER|SOUP)-\d{3}\b")

# Contrôle syntaxique d'une version épinglée : pas d'intervalle, de joker ni d'alias
# de branche ou de canal connu. Il ne garantit pas l'immuabilité de la référence :
# une étiquette peut être déplacée en amont ; la revue SOUP reste nécessaire.
PINNED_VERSION = re.compile(r"^[0-9A-Za-z][0-9A-Za-z.+_-]*$")
UNPINNED_VERSIONS = {"latest", "main", "master", "head", "develop", "trunk", "release", "stable", "nightly"}


def load(registry: Path) -> tuple[dict[str, list[dict]], list[str]]:
    """Charge chaque fichier du registre ; un fichier absent ou vide vaut une liste vide.

    Toute table racine autre que la clé attendue est refusée : une faute de nom
    (« [[requirements]] » au lieu de « [[requirement]] ») ne doit pas faire
    disparaître silencieusement les entrées de la vérification.
    """
    entries, errors = {}, []
    for filename, (key, _, _) in SCHEMA.items():
        entries[key] = []
        path = registry / filename
        if not path.exists():
            continue
        try:
            data = tomllib.loads(path.read_text(encoding="utf-8"))
        except tomllib.TOMLDecodeError as error:
            errors.append(f"{filename}: TOML invalide ({error})")
            continue
        for unknown in sorted(set(data) - {key}):
            errors.append(f"{filename}: clé racine « {unknown} » inconnue, seule « [[{key}]] » est admise")
        items = data.get(key, [])
        if not isinstance(items, list):
            errors.append(f"{filename}: « {key} » doit être un tableau de tables « [[{key}]] »")
            continue
        for index, item in enumerate(items):
            if isinstance(item, dict):
                entries[key].append(item)
            else:
                errors.append(f"{filename}: entrée {index + 1} de « {key} » n'est pas une table")
    return entries, errors


def field_error(value, kind: str) -> str | None:
    """Décrit l'écart entre une valeur et le type attendu, ou None si elle est conforme."""
    if kind == TEXT:
        if not isinstance(value, str):
            return f"doit être une chaîne, pas {type(value).__name__}"
        if not value.strip():
            return "ne doit pas être vide"
    else:
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            return "doit être une liste de chaînes"
        if not value:
            return "ne doit pas être vide"
    return None


def check(entries: dict[str, list[dict]], root: Path | None = None) -> list[str]:
    """Contrôle le registre ; avec « root », vérifie aussi que les références de preuve existent."""
    errors = []
    ids: dict[str, set[str]] = {}

    for filename, (key, pattern, fields) in SCHEMA.items():
        ids[key] = set()
        optional = OPTIONAL.get(key, {})
        for index, entry in enumerate(entries[key]):
            entry_id = entry.get("id")
            label = entry_id if isinstance(entry_id, str) and entry_id else f"entrée {index + 1}"
            where = f"{filename}: {label}"
            if not isinstance(entry_id, str) or not re.fullmatch(pattern, entry_id):
                errors.append(f"{where}: identifiant invalide, motif attendu {pattern}")
            elif entry_id in ids[key]:
                errors.append(f"{where}: identifiant en double")
            else:
                ids[key].add(entry_id)
            for field, kind in fields.items():
                if field not in entry:
                    errors.append(f"{where}: champ obligatoire « {field} » absent")
                elif problem := field_error(entry[field], kind):
                    errors.append(f"{where}: champ « {field} » {problem}")
            for field, kind in optional.items():
                if field in entry and (problem := field_error(entry[field], kind)):
                    errors.append(f"{where}: champ « {field} » {problem}")
            for field in sorted(set(entry) - {"id"} - set(fields) - set(optional)):
                errors.append(f"{where}: champ « {field} » inconnu")
            status = entry.get("status")
            if key in STATUSES and isinstance(status, str) and status and status not in STATUSES[key]:
                errors.append(f"{where}: statut « {status} » inconnu")

    def strings(entry: dict, field: str) -> list[str]:
        """Valeurs d'un champ liste, en ignorant les formes invalides déjà signalées."""
        value = entry.get(field)
        return [item for item in value if isinstance(item, str)] if isinstance(value, list) else []

    def references(key: str, field: str, target: str) -> None:
        for entry in entries[key]:
            for ref in strings(entry, field):
                if ref not in ids[target]:
                    errors.append(f"{key} {entry.get('id')}: référence « {ref} » inexistante")

    references("control", "risks", "risk")
    references("control", "requirements", "requirement")
    references("verification", "requirements", "requirement")
    references("soup", "requirements", "requirement")

    all_ids = set().union(*ids.values())
    for requirement in entries["requirement"]:
        source = requirement.get("source")
        if isinstance(source, str):
            for ref in CITED_ID.findall(source):
                if ref not in all_ids:
                    errors.append(f"requirement {requirement.get('id')}: source cite « {ref} », inexistant")

    controlled = {risk for c in entries["control"] for risk in strings(c, "risks")}
    for risk in entries["risk"]:
        if risk.get("id") in controlled:
            continue
        if risk.get("status") == "controlled":
            errors.append(f"risk {risk.get('id')}: statut « controlled » sans mesure de maîtrise")
        elif risk.get("status") != "accepted":
            errors.append(f"risk {risk.get('id')}: risque orphelin, aucune mesure de maîtrise "
                          "et statut autre que « accepted »")

    verified = {req for v in entries["verification"] for req in strings(v, "requirements")}
    on_bench = {req for v in entries["verification"] if v.get("level") in BENCH_LEVELS
                for req in strings(v, "requirements")}
    for requirement in entries["requirement"]:
        identifier = requirement.get("id")
        if identifier in verified:
            if (isinstance(identifier, str) and identifier.startswith(("SYS-", "HW-"))
                    and identifier not in on_bench and requirement.get("status") != "obsolete"):
                errors.append(f"requirement {identifier}: exigence système ou matérielle sans vérification "
                              "sur banc ni en pré-essais")
            continue
        if requirement.get("status") == "approved":
            errors.append(f"requirement {requirement.get('id')}: exigence approuvée sans vérification")
        elif requirement.get("status") != "obsolete":
            errors.append(f"requirement {requirement.get('id')}: exigence sans vérification prévue")

    for verification in entries["verification"]:
        where = f"verification {verification.get('id')}"
        method = verification.get("method")
        if isinstance(method, str) and method and method not in METHODS:
            errors.append(f"{where}: méthode « {method} » inconnue")
        level = verification.get("level")
        if isinstance(level, str) and level and level not in LEVELS:
            errors.append(f"{where}: niveau « {level} » inconnu")
        reference = verification.get("reference")
        if verification.get("status") in {"passed", "failed"} and "reference" not in verification:
            errors.append(f"{where}: statut « {verification.get('status')} » sans référence de preuve")
        if isinstance(reference, str) and reference.strip():
            problem = reference_problem(reference, root)
            if problem:
                errors.append(f"{where}: référence « {reference} » {problem}")

    for soup in entries["soup"]:
        version = soup.get("version")
        if isinstance(version, str) and version and (
            not PINNED_VERSION.match(version) or version.lower() in UNPINNED_VERSIONS
        ):
            errors.append(f"soup {soup.get('id')}: version « {version} » non épinglée")

    return errors


def reference_problem(reference: str, root: Path | None) -> str | None:
    """Motif de refus d'une référence de preuve, ou None si elle est recevable.

    La preuve est un fichier du dépôt, désigné par un chemin relatif, suivi
    éventuellement d'une ancre (« tests/a.cpp#cas »). Un chemin absolu, une
    ancre seule, un répertoire ou un chemin qui sort du dépôt, y compris par un
    lien symbolique, sont refusés. Sans racine, seule la forme est contrôlée.
    """
    path = reference.split("#", 1)[0].strip()
    if not path:
        return "ne désigne aucun fichier"
    if Path(path).is_absolute():
        return "doit être un chemin relatif au dépôt"
    if root is None:
        return None
    root = root.resolve()
    target = (root / path).resolve()
    if not target.is_relative_to(root):
        return "sort du dépôt"
    if not target.exists():
        return "introuvable dans le dépôt"
    if not target.is_file():
        return "n'est pas un fichier"
    return None


def main(argv: list[str]) -> int:
    registry = Path(argv[1]) if len(argv) > 1 else DEFAULT_REGISTRY
    if not registry.is_dir():
        print(f"Registre introuvable : {registry}", file=sys.stderr)
        return 1
    entries, errors = load(registry)
    # La racine du dépôt est le parent de « software_development_file/registry ».
    errors += check(entries, registry.resolve().parent.parent)
    for error in errors:
        print(f"erreur: {error}", file=sys.stderr)
    counts = ", ".join(f"{len(v)} {k}" for k, v in entries.items())
    print(f"Registre {'incohérent' if errors else 'cohérent'} ({counts}).")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
