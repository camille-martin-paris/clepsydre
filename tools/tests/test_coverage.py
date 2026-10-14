# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Tests de tools/coverage.py ; lancer avec : python3 -m unittest discover -s tools/tests"""

import io
import json
import stat
import sys
import tempfile
import textwrap
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import coverage  # noqa: E402

SOURCE = Path("/projet")


def region(line, column, count, file_id=0, kind=coverage.CODE_REGION):
    return [line, column, line, column + 10, count, file_id, 0, kind]


def branch(line, column, true_count, false_count, file_id=0):
    return [line, column, line, column + 5, true_count, false_count, file_id, 0, coverage.BRANCH_REGION]


def export(filename, regions=(), branches=(), filenames=None):
    path = str(SOURCE / filename)
    return {"data": [{
        "files": [{"filename": path}],
        "functions": [{"filenames": filenames or [path], "regions": list(regions), "branches": list(branches)}],
    }]}


def measured(*exports, compiled=("src/core/a.cpp",)):
    result = coverage.Coverage(compiled=set(compiled))
    for item in exports:
        result.add_export(item, SOURCE)
    return result


class ScopeTest(unittest.TestCase):
    def test_class_c_is_src_without_simulator(self):
        self.assertTrue(coverage.in_class_c("src/core/version.cpp"))
        self.assertTrue(coverage.in_class_c("src/protocol/frame.cppm"))
        self.assertFalse(coverage.in_class_c("src/sim/clock.cpp"))
        self.assertFalse(coverage.in_class_c("src/sim_console/main.cpp"))
        self.assertFalse(coverage.in_class_c("tests/version_test.cpp"))
        self.assertTrue(coverage.in_class_c("src/simulation_core.cpp"), "préfixe comparé répertoire par répertoire")

    def test_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ("src/core/a.cpp", "src/core/a.cppm", "src/core/b.hpp", "src/core/notes.md",
                         "src/sim/c.cpp", "tests/d.cpp"):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text("", encoding="utf-8")
            self.assertEqual(coverage.class_c_sources(root), ["src/core/a.cpp", "src/core/a.cppm", "src/core/b.hpp"])


class AggregationTest(unittest.TestCase):
    def test_regions_are_merged_by_location(self):
        result = measured(export("src/core/a.cpp", [region(3, 1, 0)]), export("src/core/a.cpp", [region(3, 1, 4)]))
        self.assertEqual(result.regions, {("src/core/a.cpp", 3, 1, 3, 11): 4})

    def test_only_code_regions_of_class_c_are_kept(self):
        result = measured(
            export("src/core/a.cpp", [region(1, 1, 0, kind=1), region(2, 1, 0, kind=2), region(3, 1, 0, kind=3)]),
            export("src/sim/b.cpp", [region(1, 1, 0)]),
            export("tests/c.cpp", [region(1, 1, 0)]))
        self.assertEqual(result.regions, {})

    def test_files_outside_the_source_tree_are_ignored(self):
        outside = {"data": [{"files": [{"filename": "/usr/include/c++/vector"}],
                             "functions": [{"filenames": ["/usr/include/c++/vector"],
                                            "regions": [region(1, 1, 0)], "branches": [branch(1, 1, 0, 0)]}]}]}
        result = measured(outside)
        self.assertEqual((result.regions, result.branches), ({}, {}))

    def test_region_file_is_taken_from_its_file_id(self):
        # Région d'une macro définie dans un autre fichier : fileId 1.
        item = export("src/core/a.cpp", [region(5, 1, 0, file_id=1)],
                      filenames=[str(SOURCE / "src/core/a.cpp"), str(SOURCE / "src/core/macros.hpp")])
        self.assertEqual(list(measured(item).regions), [("src/core/macros.hpp", 5, 1, 5, 11)])

    def test_branch_outcomes_are_merged(self):
        result = measured(export("src/core/a.cpp", branches=[branch(7, 2, 3, 0)]),
                          export("src/core/a.cpp", branches=[branch(7, 2, 0, 1)]))
        merged = result.branches[("src/core/a.cpp", 7, 2, 7, 7)]
        self.assertEqual((merged.true_count, merged.false_count), (3, 1))


class EvaluateTest(unittest.TestCase):
    SOURCES = ["src/core/a.cpp"]

    def test_full_coverage_passes(self):
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1)], [branch(2, 1, 1, 1)])),
                                   self.SOURCES, [])
        self.assertTrue(result.passed)
        file = result.files["src/core/a.cpp"]
        self.assertEqual((file.regions, file.regions_covered, file.branches, file.branches_covered), (1, 1, 2, 2))

    def test_missed_region_fails(self):
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1), region(4, 3, 0)])),
                                   self.SOURCES, [])
        self.assertFalse(result.passed)
        self.assertEqual(result.missed, ["src/core/a.cpp:4:3 : région jamais exécutée"])

    def test_each_branch_outcome_counts(self):
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1)], [branch(2, 5, 0, 3)])),
                                   self.SOURCES, [])
        self.assertEqual(result.missed, ["src/core/a.cpp:2:5 : issue vraie de la branche jamais prise"])
        self.assertEqual(result.files["src/core/a.cpp"].branches_covered, 1)

    def test_exclusion_covers_its_line(self):
        exclusion = coverage.Exclusion("src/core/a.cpp", 2, "code défensif inatteignable")
        result = coverage.evaluate(
            measured(export("src/core/a.cpp", [region(1, 1, 1), region(2, 9, 0)], [branch(2, 5, 1, 0)])),
            self.SOURCES, [exclusion])
        self.assertTrue(result.passed)
        self.assertEqual(result.files["src/core/a.cpp"].excluded, 2)
        self.assertEqual(result.files["src/core/a.cpp"].regions, 1)

    def test_exclusion_without_justification_fails(self):
        exclusion = coverage.Exclusion("src/core/a.cpp", 2, "")
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1), region(2, 1, 0)])),
                                   self.SOURCES, [exclusion])
        self.assertFalse(result.passed)
        self.assertEqual(result.problems, ["src/core/a.cpp:2 : exclusion sans justification"])

    def test_unused_exclusion_fails(self):
        exclusion = coverage.Exclusion("src/core/a.cpp", 3, "justification")
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(3, 1, 2)])), self.SOURCES, [exclusion])
        self.assertFalse(result.passed)
        self.assertIn("exclusion sans objet", result.problems[0])

    def test_uncompiled_unit_fails(self):
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1)]),
                                            compiled=["src/core/a.cpp", "src/core/a.cppm"]),
                                   ["src/core/a.cpp", "src/core/a.cppm", "src/core/b.cpp", "src/core/b.cppm",
                                    "src/core/c.hpp"], [])
        self.assertEqual([problem.split(" : ")[0] for problem in result.problems],
                         ["src/core/b.cpp", "src/core/b.cppm"], "un en-tête n'a pas de fichier objet")
        self.assertIn("aucun fichier objet", result.problems[0])

    def test_unlinked_module_function_fails(self):
        # Cas relevé à la relecture de #124 : fonction non inline d'un module qu'aucun test ne lie.
        # L'objet du module, mesuré avec un profil vide, apporte ses régions et branches à zéro.
        tested = export("src/core/a.cpp", [region(1, 1, 1)])
        untested_object = export("src/core/b.cppm", [region(6, 28, 0), region(10, 5, 0)], [branch(7, 9, 0, 0)])
        result = coverage.evaluate(measured(tested, untested_object, compiled=["src/core/a.cpp", "src/core/b.cppm"]),
                                   ["src/core/a.cpp", "src/core/b.cppm"], [])
        self.assertFalse(result.passed)
        file = result.files["src/core/b.cppm"]
        self.assertEqual((file.regions_covered, file.regions, file.branches_covered, file.branches), (0, 2, 0, 2))

    def test_empty_scope_fails(self):
        result = coverage.evaluate(coverage.Coverage(), [], [])
        self.assertFalse(result.passed)
        self.assertIn("aucun fichier source de classe C", result.problems[0])


class ExclusionsTest(unittest.TestCase):
    def test_markers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src/core").mkdir(parents=True)
            (root / "src/core/a.cpp").write_text(textwrap.dedent("""\
                int f(int x) {
                    if (x > 0) { // COUVERTURE-EXCLUE : code défensif inatteignable
                        return 1; /* COUVERTURE-EXCLUE: autre raison */
                    }
                    return 0; // COUVERTURE-EXCLUE
                }
                """), encoding="utf-8")
            found = [(e.line, e.justification) for e in coverage.find_exclusions(root, ["src/core/a.cpp"])]
            self.assertEqual(found, [(2, "code défensif inatteignable"), (3, "autre raison"), (5, "")])


class RenderTest(unittest.TestCase):
    def test_report(self):
        exclusion = coverage.Exclusion("src/core/a.cpp", 2, "inatteignable")
        result = coverage.evaluate(
            measured(export("src/core/a.cpp", [region(1, 1, 1), region(2, 1, 0), region(5, 1, 0)])),
            ["src/core/a.cpp"], [exclusion])
        report = coverage.render(result, "abc123")
        self.assertIn("Révision : `abc123`", report)
        self.assertIn("**non atteint**", report)
        self.assertIn("| `src/core/a.cpp` | 1/2 (50.00 %) | 0/0 (—) | 1 |", report)
        self.assertIn("- src/core/a.cpp:5:1 : région jamais exécutée", report)
        self.assertIn("- `src/core/a.cpp:2` : inatteignable", report)

    def test_no_exclusion(self):
        result = coverage.evaluate(measured(export("src/core/a.cpp", [region(1, 1, 1)])), ["src/core/a.cpp"], [])
        report = coverage.render(result, None)
        self.assertIn("atteint.", report)
        self.assertIn("Aucune.", report)
        self.assertNotIn("Révision", report)


class MainTest(unittest.TestCase):
    """Chaîne complète, avec des llvm-profdata et llvm-cov simulés par des scripts."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.source = self.root / "source"
        (self.source / "src/core").mkdir(parents=True)
        (self.source / "src/core/a.cpp").write_text("int f() { return 1; }\n", encoding="utf-8")
        (self.source / "src/core/a.cppm").write_text("export module a;\nexport int f();\n", encoding="utf-8")
        self.build = self.root / "build"
        (self.build / "coverage/a_test").mkdir(parents=True)
        (self.build / "coverage/a_test/1.profraw").write_bytes(b"")
        (self.build / "coverage/binaries.txt").write_text("a_test\t/bin/a_test\n", encoding="utf-8")
        self.objects = self.build / "src/core/CMakeFiles/core.dir"
        self.objects.mkdir(parents=True)
        # a.cpp.o porte la fonction f ; a.cppm.o, interface sans code, n'a pas de données de couverture.
        self.add_object("a.cpp", [region(1, 9, 0)])
        self.add_object("a.cppm", None)
        self.profdata = self.tool("llvm-profdata", 'out="${@: -1}"; : > "$out"')
        self.export = self.root / "export.json"

    def add_object(self, name: str, regions: list | None) -> None:
        path = self.objects / f"{name}.o"
        path.write_bytes(b"")
        if regions is not None:
            self.write_export(self.objects / f"{name}.json", f"src/core/{name}", regions)
        with (self.build / "coverage/objects.txt").open("a", encoding="utf-8") as manifest:
            manifest.write(f"{self.objects}/./{name}.o\n")

    def write_export(self, path: Path, filename: str, regions: list) -> None:
        data = export(filename, regions)
        data["data"][0]["files"][0]["filename"] = str(self.source / filename)
        data["data"][0]["functions"][0]["filenames"] = [str(self.source / filename)]
        path.write_text(json.dumps(data), encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def tool(self, name: str, body: str) -> Path:
        path = self.root / name
        path.write_text(f"#!/bin/bash\n{body}\n", encoding="utf-8")
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def run_main(self, llvm_cov: Path, *extra: str) -> tuple[int, str, str]:
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = coverage.main(["coverage.py", "--build-dir", str(self.build), "--source-dir", str(self.source),
                                  "--llvm-cov", str(llvm_cov), "--llvm-profdata", str(self.profdata), *extra])
        return code, stdout.getvalue(), stderr.getvalue()

    def llvm_cov(self, count: int, stderr: str = "") -> Path:
        """llvm-cov simulé : un objet renvoie son export voisin (.json), ou « no coverage data found »
        s'il n'en a pas ; l'exécutable de test renvoie un export où f est exécutée count fois."""
        self.write_export(self.export, "src/core/a.cpp", [region(1, 9, count)])
        return self.tool("llvm-cov", textwrap.dedent(f"""\
            last="${{@: -1}}"
            case "$last" in
              *.o)
                if [ -f "${{last%.o}}.json" ]; then cat "${{last%.o}}.json"; exit 0; fi
                echo "error: failed to load coverage: '$last': no coverage data found" >&2; exit 1 ;;
              *) cat "{self.export}"; echo "{stderr}" >&2 ;;
            esac"""))

    def test_covered(self):
        output = self.root / "rapport.md"
        code, stdout, _ = self.run_main(self.llvm_cov(1), "--revision", "abc", "-o", str(output))
        self.assertEqual(code, 0, stdout)
        self.assertIn("atteint.", stdout)
        self.assertEqual(output.read_text(encoding="utf-8"), stdout)

    def test_not_covered(self):
        code, stdout, _ = self.run_main(self.llvm_cov(0))
        self.assertEqual(code, 1)
        self.assertIn("src/core/a.cpp:1:9 : région jamais exécutée", stdout)

    def test_unlinked_module_function_fails(self):
        # Module compilé dans une cible de src/ mais lié par aucun test : présent par son objet.
        (self.source / "src/core/b.cppm").write_text("export module b;\nexport int g() { return 2; }\n",
                                                    encoding="utf-8")
        self.add_object("b.cppm", [region(2, 17, 0)])
        code, stdout, _ = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 1)
        self.assertIn("src/core/b.cppm:2:17 : région jamais exécutée", stdout)

    def test_uncompiled_unit_fails(self):
        (self.source / "src/core/c.cppm").write_text("export module c;\n", encoding="utf-8")
        code, stdout, _ = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 1)
        self.assertIn("src/core/c.cppm : aucun fichier objet", stdout)

    def test_object_tool_error_is_a_measure_error(self):
        code, _, stderr = self.run_main(self.tool("llvm-cov", 'echo "error: malformed object" >&2; exit 1'))
        self.assertEqual(code, 2)
        self.assertIn("malformed object", stderr)

    def test_missing_object_file(self):
        (self.objects / "a.cpp.o").unlink()
        code, _, stderr = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 2)
        self.assertIn("fichier objet absent", stderr)

    def test_report_written_when_measure_is_impossible(self):
        # Remarque de relecture de #124 : sans rapport, l'envoi de l'artefact échouait à son tour et
        # masquait la vraie erreur.
        output = self.root / "rapport.md"
        code, _, _ = self.run_main(self.tool("llvm-cov", "exit 3"), "--revision", "abc", "-o", str(output))
        self.assertEqual(code, 2)
        report = output.read_text(encoding="utf-8")
        self.assertIn("Révision : `abc`", report)
        self.assertIn("**Mesure impossible**", report)
        self.assertIn("a échoué (code 3)", report)

    def test_missing_objects_manifest(self):
        (self.build / "coverage/objects.txt").unlink()
        code, _, stderr = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 2)
        self.assertIn("objects.txt absent", stderr)

    def test_object_source(self):
        build = self.build.resolve()
        self.assertEqual(coverage.object_source(build, build / "src/core/CMakeFiles/core.dir/./a.cppm.o"),
                         "src/core/a.cppm")
        self.assertEqual(coverage.object_source(build, build / "src/x/CMakeFiles/x.dir/sub/b.cpp.o"), "src/x/sub/b.cpp")
        self.assertIsNone(coverage.object_source(build, build / "src/core/a.cpp.o"))
        self.assertIsNone(coverage.object_source(build, Path("/ailleurs/CMakeFiles/x.dir/a.cpp.o")))
        self.assertIsNone(coverage.object_source(build, build / "src/CMakeFiles/x.dir/a.cpp.obj"))

    def test_mismatched_profile_is_a_measure_error(self):
        code, _, stderr = self.run_main(self.llvm_cov(1, "warning: 2 functions have mismatched data"))
        self.assertEqual(code, 2)
        self.assertIn("profils incohérents", stderr)

    def test_failing_tool_is_a_measure_error(self):
        code, _, stderr = self.run_main(self.tool("llvm-cov", "exit 3"))
        self.assertEqual(code, 2)
        self.assertIn("a échoué (code 3)", stderr)

    def test_missing_tool_is_a_measure_error(self):
        code, _, stderr = self.run_main(self.root / "absent")
        self.assertEqual(code, 2)
        self.assertIn("Mesure impossible", stderr)

    def test_missing_profiles(self):
        (self.build / "coverage/a_test/1.profraw").unlink()
        code, _, stderr = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 2)
        self.assertIn("aucun profil pour a_test", stderr)

    def test_missing_manifest(self):
        (self.build / "coverage/binaries.txt").unlink()
        code, _, stderr = self.run_main(self.llvm_cov(1))
        self.assertEqual(code, 2)
        self.assertIn("CLEPSYDRE_COVERAGE=ON", stderr)


if __name__ == "__main__":
    unittest.main()
