#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Mesure la couverture du code de classe C et la compare au critère de 100 %.

Le build est configuré avec CLEPSYDRE_COVERAGE=ON (Clang) et les tests ont été
exécutés par CTest. Chaque exécutable mesuré est listé dans
<build>/coverage/binaries.txt, et ses profils d'exécution sont rangés dans
<build>/coverage/<cible>/. Pour chaque exécutable, le script fusionne ses
profils (llvm-profdata), exporte sa couverture (llvm-cov export), puis réunit
les résultats de tous les exécutables par emplacement dans le source.

Les fichiers objets des cibles de src/, listés dans <build>/coverage/objects.txt,
sont aussi exportés, avec un profil vide : une fonction compilée qu'aucun
exécutable de test ne lie figure ainsi dans la mesure, comme non couverte. Chaque
unité .cpp ou .cppm de classe C doit avoir un fichier objet ; un objet sans code
exécutable (interface qui ne contient que des déclarations) n'a pas de données
de couverture et n'ajoute rien.

Critère : 100 % des régions (instructions) et des deux issues de chaque branche
du code de classe C. Le code de classe C est tout le code de src/, sauf les
répertoires de NOT_EMBEDDED, qui ne sont pas embarqués.

Exclusion : un marqueur « COUVERTURE-EXCLUE : <justification> » dans un
commentaire exclut les régions et les branches qui commencent sur sa ligne. Le
contrôle échoue si un marqueur n'a pas de justification, ou s'il n'exclut rien
(marqueur sans objet).

Usage : coverage.py --build-dir DIR [--source-dir DIR] [--llvm-cov EXE]
                    [--llvm-profdata EXE] [-o RAPPORT.md]
Code de retour : 0 si le critère est atteint, 1 sinon, 2 si la mesure est impossible ; le
rapport (-o) est écrit dans les trois cas.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLASS_C_ROOT = "src/"
NOT_EMBEDDED = ("src/sim/", "src/sim_console/")
SOURCE_SUFFIXES = (".cpp", ".cppm", ".hpp")
MARKER = re.compile(r"COUVERTURE-EXCLUE\b\s*:?\s*(.*?)\s*(?:\*/\s*)?$")

# Genres de région de llvm-cov export.
CODE_REGION = 0
BRANCH_REGION = 4


class MeasureError(Exception):
    """La mesure ne peut pas être faite ; ce n'est pas un résultat de couverture."""


def in_class_c(path: str) -> bool:
    return path.startswith(CLASS_C_ROOT) and not path.startswith(NOT_EMBEDDED)


def class_c_sources(source_dir: Path) -> list[str]:
    sources = []
    for path in sorted((source_dir / CLASS_C_ROOT).rglob("*")):
        relative = path.relative_to(source_dir).as_posix()
        if path.is_file() and path.suffix in SOURCE_SUFFIXES and in_class_c(relative):
            sources.append(relative)
    return sources


@dataclass
class Branch:
    true_count: int = 0
    false_count: int = 0


@dataclass
class Coverage:
    """Couverture réunie de tous les exécutables, par emplacement (fichier, début, fin)."""

    regions: dict[tuple, int] = field(default_factory=dict)
    branches: dict[tuple, Branch] = field(default_factory=dict)
    compiled: set[str] = field(default_factory=set)  # sources de classe C qui ont un fichier objet

    def add_export(self, export: dict, source_dir: Path) -> None:
        def relative(filename: str) -> str | None:
            try:
                return Path(filename).resolve().relative_to(source_dir).as_posix()
            except ValueError:
                return None

        for data in export["data"]:
            for function in data["functions"]:
                names = [relative(filename) for filename in function["filenames"]]
                for line_start, col_start, line_end, col_end, count, file_id, _, kind in function["regions"]:
                    name = names[file_id]
                    if kind == CODE_REGION and name is not None and in_class_c(name):
                        key = (name, line_start, col_start, line_end, col_end)
                        self.regions[key] = max(self.regions.get(key, 0), count)
                for branch in function["branches"]:
                    line_start, col_start, line_end, col_end, true_count, false_count, file_id = branch[:7]
                    name = names[file_id]
                    if name is not None and in_class_c(name):
                        key = (name, line_start, col_start, line_end, col_end)
                        merged = self.branches.setdefault(key, Branch())
                        merged.true_count = max(merged.true_count, true_count)
                        merged.false_count = max(merged.false_count, false_count)


@dataclass
class Exclusion:
    path: str
    line: int
    justification: str
    used: bool = False


def find_exclusions(source_dir: Path, sources: list[str]) -> list[Exclusion]:
    exclusions = []
    for path in sources:
        text = (source_dir / path).read_text(encoding="utf-8")
        for number, line in enumerate(text.splitlines(), start=1):
            match = MARKER.search(line)
            if match:
                exclusions.append(Exclusion(path, number, match.group(1)))
    return exclusions


@dataclass
class FileResult:
    regions: int = 0
    regions_covered: int = 0
    branches: int = 0
    branches_covered: int = 0
    excluded: int = 0


@dataclass
class Result:
    files: dict[str, FileResult]
    missed: list[str]
    exclusions: list[Exclusion]
    problems: list[str]

    @property
    def passed(self) -> bool:
        return not self.missed and not self.problems


def evaluate(coverage: Coverage, sources: list[str], exclusions: list[Exclusion]) -> Result:
    files = {path: FileResult() for path in sources}
    by_line = {(exclusion.path, exclusion.line): exclusion for exclusion in exclusions}
    missed = []
    problems = []

    def excluded(path: str, line: int) -> bool:
        exclusion = by_line.get((path, line))
        if exclusion is None:
            return False
        exclusion.used = True
        files[path].excluded += 1
        return True

    for (path, line, column, _, _), count in sorted(coverage.regions.items()):
        result = files.setdefault(path, FileResult())
        if count > 0:
            result.regions += 1
            result.regions_covered += 1
        elif not excluded(path, line):
            result.regions += 1
            missed.append(f"{path}:{line}:{column} : région jamais exécutée")
    for (path, line, column, _, _), branch in sorted(coverage.branches.items()):
        result = files.setdefault(path, FileResult())
        for outcome, count in (("vraie", branch.true_count), ("fausse", branch.false_count)):
            if count > 0:
                result.branches += 1
                result.branches_covered += 1
            elif not excluded(path, line):
                result.branches += 1
                missed.append(f"{path}:{line}:{column} : issue {outcome} de la branche jamais prise")

    if not sources:
        problems.append(f"aucun fichier source de classe C sous {CLASS_C_ROOT} : la mesure serait vide")
    for path in sources:
        # Sans fichier objet, le code de l'unité échapperait à la mesure des objets.
        if path.endswith((".cpp", ".cppm")) and path not in coverage.compiled:
            problems.append(f"{path} : aucun fichier objet (unité compilée dans aucune cible de src/ "
                            "déclarée par clepsydre_register_objects)")
    for exclusion in exclusions:
        if not exclusion.justification:
            problems.append(f"{exclusion.path}:{exclusion.line} : exclusion sans justification")
        if not exclusion.used:
            problems.append(f"{exclusion.path}:{exclusion.line} : exclusion sans objet "
                            "(aucune région ni branche non couverte sur cette ligne)")
    return Result(files, missed, exclusions, problems)


def percent(covered: int, total: int) -> str:
    return "—" if total == 0 else f"{100.0 * covered / total:.2f} %"


def render(result: Result, revision: str | None) -> str:
    lines = ["## Couverture du code de classe C", ""]
    if revision:
        lines += [f"Révision : `{revision}`", ""]
    verdict = "atteint" if result.passed else "**non atteint**"
    lines += [f"Critère de 100 % des régions et des branches : {verdict}.", ""]
    lines += ["| Fichier | Régions | Branches (issues) | Exclusions |", "| --- | --- | --- | --- |"]
    total = FileResult()
    for path, file in sorted(result.files.items()):
        lines.append(f"| `{path}` | {file.regions_covered}/{file.regions} ({percent(file.regions_covered, file.regions)})"
                     f" | {file.branches_covered}/{file.branches} ({percent(file.branches_covered, file.branches)})"
                     f" | {file.excluded} |")
        total.regions += file.regions
        total.regions_covered += file.regions_covered
        total.branches += file.branches
        total.branches_covered += file.branches_covered
        total.excluded += file.excluded
    lines.append(f"| **Total** | {total.regions_covered}/{total.regions} ({percent(total.regions_covered, total.regions)})"
                 f" | {total.branches_covered}/{total.branches} ({percent(total.branches_covered, total.branches)})"
                 f" | {total.excluded} |")
    for title, items in (("Non couvert", result.missed), ("Anomalies", result.problems)):
        if items:
            lines += ["", f"### {title}", ""] + [f"- {item}" for item in items]
    lines += ["", "### Exclusions", ""]
    if result.exclusions:
        lines += [f"- `{e.path}:{e.line}` : {e.justification or '(sans justification)'}" for e in result.exclusions]
    else:
        lines.append("Aucune.")
    return "\n".join(lines) + "\n"


NO_COVERAGE_DATA = "no coverage data found"


def render_impossible(error: MeasureError, revision: str | None) -> str:
    lines = ["## Couverture du code de classe C", ""]
    if revision:
        lines += [f"Révision : `{revision}`", ""]
    lines += ["**Mesure impossible** : le critère n'a pas pu être évalué.", "", "```text", str(error), "```"]
    return "\n".join(lines) + "\n"


def run(command: list[str], *, allow_no_data: bool = False) -> str | None:
    """Exécute un outil LLVM ; None si allow_no_data et que l'objet n'a pas de données de couverture."""
    # Sans DEBUGINFOD_URLS, llvm-cov n'interroge pas le réseau : sur un poste où la variable est
    # définie, il peut attendre indéfiniment un serveur de symboles.
    environment = {**os.environ, "DEBUGINFOD_URLS": ""}
    try:
        completed = subprocess.run(command, capture_output=True, text=True, env=environment, check=False)
    except OSError as error:
        raise MeasureError(f"{command[0]} : {error}") from error
    if completed.returncode != 0 and allow_no_data and NO_COVERAGE_DATA in completed.stderr:
        return None
    if completed.returncode != 0:
        raise MeasureError(f"{' '.join(command)} a échoué (code {completed.returncode}) :\n{completed.stderr}")
    if "mismatched data" in completed.stderr:
        raise MeasureError(f"{' '.join(command)} : profils incohérents avec l'exécutable :\n{completed.stderr}")
    return completed.stdout


def object_source(build_dir: Path, path: Path) -> str | None:
    """Source d'un fichier objet : <build>/<rép>/CMakeFiles/<cible>.dir/<fichier>.o vient de <rép>/<fichier>."""
    try:
        parts = path.resolve().relative_to(build_dir).parts
        index = parts.index("CMakeFiles")
    except ValueError:
        return None
    if index + 2 >= len(parts) or not parts[-1].endswith(".o"):
        return None
    source = Path(*parts[:index], *parts[index + 2:])
    return source.with_name(source.name.removesuffix(".o")).as_posix()


def measure(build_dir: Path, source_dir: Path, llvm_cov: str, llvm_profdata: str) -> Coverage:
    coverage_dir = build_dir / "coverage"
    manifest = coverage_dir / "binaries.txt"
    objects = coverage_dir / "objects.txt"
    for required in (manifest, objects):
        if not required.is_file():
            raise MeasureError(f"{required} absent : configurer le build avec -DCLEPSYDRE_COVERAGE=ON")
    coverage = Coverage()
    with tempfile.TemporaryDirectory() as scratch:
        empty_text = Path(scratch, "vide.proftext")
        empty_text.write_text("", encoding="utf-8")
        empty = Path(scratch, "vide.profdata")
        run([llvm_profdata, "merge", str(empty_text), "-o", str(empty)])
        for line in objects.read_text(encoding="utf-8").splitlines():
            if not line:
                continue
            path = Path(line)
            if not path.is_file():
                raise MeasureError(f"fichier objet absent : {path} (build incomplet ?)")
            source = object_source(build_dir, path)
            if source is not None and in_class_c(source):
                coverage.compiled.add(source)
            export = run([llvm_cov, "export", "-format=text", f"-instr-profile={empty}", str(path)],
                         allow_no_data=True)
            if export is not None:
                coverage.add_export(json.loads(export), source_dir)
        for line in manifest.read_text(encoding="utf-8").splitlines():
            target, executable = line.split("\t")
            profiles = sorted((coverage_dir / target).glob("*.profraw"))
            if not profiles:
                raise MeasureError(f"aucun profil pour {target} : les tests ont-ils été exécutés ?")
            profdata = Path(scratch, f"{target}.profdata")
            run([llvm_profdata, "merge", "-sparse", *map(str, profiles), "-o", str(profdata)])
            export = run([llvm_cov, "export", "-format=text", f"-instr-profile={profdata}", executable])
            coverage.add_export(json.loads(export), source_dir)
    return coverage


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, default=ROOT)
    parser.add_argument("--llvm-cov", default="llvm-cov")
    parser.add_argument("--llvm-profdata", default="llvm-profdata")
    parser.add_argument("--revision")
    parser.add_argument("-o", "--output", type=Path, help="rapport Markdown")
    args = parser.parse_args(argv[1:])
    source_dir = args.source_dir.resolve()
    try:
        coverage = measure(args.build_dir.resolve(), source_dir, args.llvm_cov, args.llvm_profdata)
    except MeasureError as error:
        print(f"Mesure impossible : {error}", file=sys.stderr)
        # Le rapport documente aussi l'échec de la mesure : la CI le publie et l'archive.
        if args.output:
            args.output.write_text(render_impossible(error, args.revision), encoding="utf-8")
        return 2
    sources = class_c_sources(source_dir)
    result = evaluate(coverage, sources, find_exclusions(source_dir, sources))
    report = render(result, args.revision)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
