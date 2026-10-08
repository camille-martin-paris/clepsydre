#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Vérifie la cohérence du registre de traçabilité de Clepsydre.

Le registre (software_development_file/registry/) relie exigences, risques,
mesures de maîtrise, vérifications et composants tiers (SOUP). Le script
signale les identifiants mal formés ou en double, les références vers des
éléments inexistants et les chaînes de traçabilité incomplètes.

Usage : check_registry.py [répertoire_du_registre]
Code de retour : 0 si le registre est cohérent, 1 sinon.
"""

import re
import sys
import tomllib
from pathlib import Path

DEFAULT_REGISTRY = Path(__file__).resolve().parent.parent / "software_development_file" / "registry"

# Pour chaque fichier : clé des entrées, motif d'identifiant, champs obligatoires.
SCHEMA = {
    "requirements.toml": ("requirement", r"(SYS|SW|HW)-REQ-\d{3}", ("title", "text", "source", "status")),
    "risks.toml": ("risk", r"RISK-\d{3}", ("hazard", "harm", "status")),
    "controls.toml": ("control", r"CTRL-\d{3}", ("description", "risks", "requirements")),
    "verifications.toml": ("verification", r"VER-\d{3}", ("method", "requirements", "status")),
    "soup.toml": ("soup", r"SOUP-\d{3}", ("name", "version", "usage", "requirements", "known_anomalies")),
}

STATUSES = {
    "requirement": {"draft", "approved", "obsolete"},
    "risk": {"draft", "controlled", "accepted"},
    "verification": {"planned", "passed", "failed"},
}

METHODS = {"test", "analysis", "inspection", "demonstration"}

# Une version épinglée est exacte : pas d'intervalle, de joker ni de branche.
PINNED_VERSION = re.compile(r"^[0-9A-Za-z][0-9A-Za-z.+_-]*$")
UNPINNED_VERSIONS = {"latest", "main", "master", "head", "develop"}


def load(registry: Path) -> tuple[dict[str, list[dict]], list[str]]:
    """Charge chaque fichier du registre ; un fichier absent vaut une liste vide."""
    entries, errors = {}, []
    for filename, (key, _, _) in SCHEMA.items():
        path = registry / filename
        if not path.exists():
            entries[key] = []
            continue
        try:
            data = tomllib.loads(path.read_text(encoding="utf-8"))
        except tomllib.TOMLDecodeError as error:
            errors.append(f"{filename}: TOML invalide ({error})")
            data = {}
        entries[key] = data.get(key, [])
    return entries, errors


def check(entries: dict[str, list[dict]]) -> list[str]:
    errors = []
    ids: dict[str, set[str]] = {}

    for filename, (key, pattern, fields) in SCHEMA.items():
        ids[key] = set()
        for index, entry in enumerate(entries[key]):
            entry_id = entry.get("id", "")
            where = f"{filename}: {entry_id or f'entrée {index + 1}'}"
            if not re.fullmatch(pattern, entry_id):
                errors.append(f"{where}: identifiant invalide, motif attendu {pattern}")
            elif entry_id in ids[key]:
                errors.append(f"{where}: identifiant en double")
            ids[key].add(entry_id)
            for field in fields:
                if field not in entry or entry[field] in ("", None):
                    errors.append(f"{where}: champ obligatoire « {field} » absent")
            if key in STATUSES and entry.get("status") not in STATUSES[key] | {None, ""}:
                errors.append(f"{where}: statut « {entry['status']} » inconnu")

    def references(key: str, field: str, target: str) -> None:
        for entry in entries[key]:
            for ref in entry.get(field, []):
                if ref not in ids[target]:
                    errors.append(f"{key} {entry.get('id')}: référence « {ref} » inexistante")

    references("control", "risks", "risk")
    references("control", "requirements", "requirement")
    references("verification", "requirements", "requirement")
    references("soup", "requirements", "requirement")

    for control in entries["control"]:
        for field in ("risks", "requirements"):
            if not control.get(field):
                errors.append(f"control {control.get('id')}: « {field} » ne doit pas être vide")

    controlled = {risk for c in entries["control"] for risk in c.get("risks", [])}
    for risk in entries["risk"]:
        if risk.get("status") == "controlled" and risk.get("id") not in controlled:
            errors.append(f"risk {risk.get('id')}: statut « controlled » sans mesure de maîtrise")

    verified = {req for v in entries["verification"] for req in v.get("requirements", [])}
    for requirement in entries["requirement"]:
        if requirement.get("status") == "approved" and requirement.get("id") not in verified:
            errors.append(f"requirement {requirement.get('id')}: exigence approuvée sans vérification")

    for verification in entries["verification"]:
        if verification.get("method") not in METHODS | {None, ""}:
            errors.append(f"verification {verification.get('id')}: méthode « {verification['method']} » inconnue")

    for soup in entries["soup"]:
        version = str(soup.get("version", ""))
        if version and (not PINNED_VERSION.match(version) or version.lower() in UNPINNED_VERSIONS):
            errors.append(f"soup {soup.get('id')}: version « {version} » non épinglée")

    return errors


def main(argv: list[str]) -> int:
    registry = Path(argv[1]) if len(argv) > 1 else DEFAULT_REGISTRY
    if not registry.is_dir():
        print(f"Registre introuvable : {registry}", file=sys.stderr)
        return 1
    entries, errors = load(registry)
    errors += check(entries)
    for error in errors:
        print(f"erreur: {error}", file=sys.stderr)
    counts = ", ".join(f"{len(v)} {k}" for k, v in entries.items())
    print(f"Registre {'incohérent' if errors else 'cohérent'} ({counts}).")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
