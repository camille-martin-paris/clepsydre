#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Génère la matrice de traçabilité de Clepsydre en Markdown.

La matrice relie risques, mesures de maîtrise, exigences et vérifications du
registre (software_development_file/registry/). Elle n'est produite que si le
registre est cohérent au sens de check_registry.py.

Un lien entre une exigence et une vérification indique qu'une vérification
est prévue ou rattachée ; seul le statut « passed », avec une référence de
preuve, constitue un résultat.

Usage : traceability_matrix.py [--registry RÉPERTOIRE] [--revision RÉVISION] [-o FICHIER]
Code de retour : 0 si la matrice est produite, 1 si le registre est incohérent.
"""

import argparse
import sys
from collections import Counter
from pathlib import Path

import check_registry

STATUS_LABELS = {"planned": "prévue", "passed": "réussie", "failed": "échouée"}
LEVEL_LABELS = {"unit": "unitaire", "integration": "intégration", "system": "système logiciel",
                "bench": "banc", "precompliance": "pré-essai normatif"}
METHOD_LABELS = {"test": "essai", "analysis": "analyse", "inspection": "inspection",
                 "demonstration": "démonstration"}


def requirement_state(verifications: list[dict]) -> str:
    """État de vérification d'une exigence, déduit des seuls statuts de ses vérifications."""
    statuses = {v["status"] for v in verifications}
    if not verifications:
        return "Aucune vérification"
    if "failed" in statuses:
        return "**Échec**"
    if statuses == {"passed"}:
        return "Vérifiée"
    if "passed" in statuses:
        return "Partiellement vérifiée"
    return "Non vérifiée (vérifications prévues)"


def verification_label(v: dict) -> str:
    label = (f"{v['id']} ({METHOD_LABELS.get(v['method'], v['method'])}, "
             f"{LEVEL_LABELS.get(v['level'], v['level'])}, {STATUS_LABELS.get(v['status'], v['status'])}")
    if v.get("reference"):
        label += f", `{v['reference']}`"
    return label + ")"


def render(entries: dict[str, list[dict]], revision: str | None = None) -> str:
    requirements = entries["requirement"]
    verifications = entries["verification"]
    by_requirement: dict[str, list[dict]] = {r["id"]: [] for r in requirements}
    for v in verifications:
        for rid in v["requirements"]:
            by_requirement[rid].append(v)
    controls_of: dict[str, list[str]] = {r["id"]: [] for r in requirements}
    for c in entries["control"]:
        for rid in c["requirements"]:
            controls_of[rid].append(c["id"])
    states = {rid: requirement_state(vs) for rid, vs in by_requirement.items()}

    out = ["# Matrice de traçabilité", ""]
    if revision:
        out += [f"Révision du registre : `{revision}`.", ""]
    out += [
        "> [!IMPORTANT]",
        "> Cette matrice est générée à partir du registre. Un lien entre une exigence et une vérification "
        "signifie qu'une vérification est **prévue ou rattachée**, pas qu'elle a réussi. "
        "Seule une vérification au statut « réussie », avec une référence de preuve, constitue un résultat.",
        "",
        "## Synthèse",
        "",
        "| Élément | Nombre |",
        "| --- | --- |",
        f"| Risques | {len(entries['risk'])} |",
        f"| Mesures de maîtrise | {len(entries['control'])} |",
        f"| Exigences | {len(requirements)} |",
        f"| Vérifications | {len(verifications)} |",
        f"| Composants tiers (SOUP) | {len(entries['soup'])} |",
        "",
        "| Exigences par état de vérification | Nombre |",
        "| --- | --- |",
    ]
    for state, count in sorted(Counter(states.values()).items()):
        out.append(f"| {state} | {count} |")
    out += ["", "| Vérifications par niveau et statut | " + " | ".join(STATUS_LABELS.values()) + " |",
            "| --- |" + " --- |" * len(STATUS_LABELS)]
    for level, label in LEVEL_LABELS.items():
        counts = Counter(v["status"] for v in verifications if v["level"] == level)
        out.append(f"| {label} | " + " | ".join(str(counts[s]) for s in STATUS_LABELS) + " |")

    out += ["", "## Risques, mesures et exigences", "",
            "| Risque | Statut | Mesures | Exigences | État de vérification des exigences |",
            "| --- | --- | --- | --- | --- |"]
    for risk in entries["risk"]:
        controls = [c for c in entries["control"] if risk["id"] in c["risks"]]
        rids = sorted({rid for c in controls for rid in c["requirements"]}, key=sort_key)
        state = Counter(states[rid] for rid in rids)
        summary = ", ".join(f"{s.lower()} : {n}" for s, n in sorted(state.items())) or "—"
        out.append(f"| {risk['id']} | {risk['status']} | {', '.join(c['id'] for c in controls) or '—'} | "
                   f"{', '.join(rids) or '—'} | {summary} |")

    out += ["", "## Exigences et vérifications", "",
            "| Exigence | Statut | Mesures | Source | Vérifications | État |",
            "| --- | --- | --- | --- | --- | --- |"]
    for r in sorted(requirements, key=lambda r: sort_key(r["id"])):
        cited = check_registry.CITED_ID.findall(r["source"])
        out.append(f"| {r['id']} | {r['status']} | {', '.join(controls_of[r['id']]) or '—'} | "
                   f"{', '.join(cited) or '—'} | "
                   f"{'<br>'.join(verification_label(v) for v in by_requirement[r['id']]) or '—'} | "
                   f"{states[r['id']]} |")
    if entries["soup"]:
        out += ["", "## Composants tiers", "", "| SOUP | Nom | Version | Exigences |", "| --- | --- | --- | --- |"]
        for s in entries["soup"]:
            out.append(f"| {s['id']} | {s['name']} | {s['version']} | {', '.join(s['requirements'])} |")
    return "\n".join(out) + "\n"


def sort_key(rid: str) -> tuple[int, str]:
    return ({"SYS": 0, "SW": 1, "HW": 2}.get(rid.split("-")[0], 3), rid)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--registry", type=Path, default=check_registry.DEFAULT_REGISTRY)
    parser.add_argument("--revision", help="révision Git du registre, reprise dans l'en-tête")
    parser.add_argument("-o", "--output", type=Path, help="fichier de sortie (sortie standard par défaut)")
    args = parser.parse_args(argv[1:])

    entries, errors = check_registry.load(args.registry)
    errors += check_registry.check(entries, args.registry.resolve().parent.parent)
    if errors:
        for error in errors:
            print(f"erreur: {error}", file=sys.stderr)
        print("Registre incohérent : matrice non générée.", file=sys.stderr)
        return 1
    matrix = render(entries, args.revision)
    if args.output:
        args.output.write_text(matrix, encoding="utf-8")
    else:
        sys.stdout.write(matrix)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
