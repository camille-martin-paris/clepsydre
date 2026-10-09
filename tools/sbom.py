#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Génère la nomenclature logicielle (SBOM) de Clepsydre au format CycloneDX 1.6 (JSON).

La SBOM décrit le logiciel embarqué et ses composants tiers (SOUP), lus dans
software_development_file/registry/soup.toml. Elle n'est générée que si le
registre est cohérent. Les outils de build et de vérification n'en font pas
partie : ils sont épinglés dans la CI (tools/check_pins.py).

La sortie est reproductible : l'horodatage est la date du commit et le numéro
de série est dérivé du commit et de la version.

Usage : sbom.py [--registry DIR] [--version V] [--commit SHA] [--timestamp ISO] [-o FICHIER]
Code de retour : 0 si la SBOM est écrite, 1 sinon.
"""

import argparse
import json
import subprocess
import sys
import uuid
from pathlib import Path

import check_registry

ROOT = Path(__file__).resolve().parent.parent
REPOSITORY = "https://github.com/camille-martin-paris/clepsydre"
PRODUCT_LICENSE = "EUPL-1.2"


def git(*args: str) -> str | None:
    try:
        result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


def render(soups: list[dict], version: str, commit: str, timestamp: str) -> dict:
    components = []
    for soup in soups:
        component = {
            "type": "library",
            "bom-ref": soup["id"],
            "name": soup["name"],
            "version": soup["version"],
            "supplier": {"name": soup["supplier"]},
            "licenses": [{"expression": soup["license"]}],
            "description": soup["usage"],
            "properties": [{"name": "clepsydre:requirements", "value": ", ".join(soup["requirements"])}],
        }
        if "purl" in soup:
            component["purl"] = soup["purl"]
        components.append(component)
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, f'{REPOSITORY}@{commit}#{version}')}",
        "version": 1,
        "metadata": {
            "timestamp": timestamp,
            "component": {
                "type": "firmware",
                "bom-ref": "clepsydre",
                "name": "Clepsydre",
                "version": version,
                "licenses": [{"license": {"id": PRODUCT_LICENSE}}],
                "externalReferences": [{"type": "vcs", "url": REPOSITORY}],
                "properties": [{"name": "clepsydre:commit", "value": commit}],
            },
        },
        "components": components,
        "dependencies": [
            {"ref": "clepsydre", "dependsOn": [soup["id"] for soup in soups]},
            *({"ref": soup["id"], "dependsOn": []} for soup in soups),
        ],
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--registry", type=Path, default=check_registry.DEFAULT_REGISTRY)
    parser.add_argument("--version", help="version publiée ; par défaut, git describe")
    parser.add_argument("--commit", help="commit décrit ; par défaut, HEAD")
    parser.add_argument("--timestamp", help="date ISO 8601 ; par défaut, date du commit")
    parser.add_argument("-o", "--output", type=Path, help="fichier de sortie ; par défaut, sortie standard")
    args = parser.parse_args(argv[1:])

    if not args.registry.is_dir():
        print(f"Registre introuvable : {args.registry}", file=sys.stderr)
        return 1
    entries, errors = check_registry.load(args.registry)
    errors += check_registry.check(entries, args.registry.resolve().parent.parent)
    if errors:
        for error in errors:
            print(f"erreur: {error}", file=sys.stderr)
        print("Registre incohérent : SBOM non générée.", file=sys.stderr)
        return 1

    commit = args.commit or git("rev-parse", "HEAD")
    timestamp = args.timestamp or (commit and git("log", "-1", "--format=%cI", commit))
    if not commit or not timestamp:
        print("Commit ou date inconnus : passer --commit et --timestamp hors d'un dépôt Git.", file=sys.stderr)
        return 1
    version = args.version or git("describe", "--tags", "--always", commit) or commit

    text = json.dumps(render(entries["soup"], version, commit, timestamp), ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
