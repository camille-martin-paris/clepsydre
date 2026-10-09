# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Tests de tools/check_pins.py ; lancer avec : python3 -m unittest discover -s tools/tests"""

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_pins  # noqa: E402

COMMIT = "11bd71901bbe5b1630ceea73d27597364c9af683"
DIGEST = "sha256:" + "f" * 64
HASH = "--hash=sha256:" + "a" * 64

PINNED_WORKFLOW = f"""name: essai
env:
  PYTHON_VERSION: "3.12.14"
jobs:
  build:
    runs-on: ubuntu-24.04
    container: ubuntu:26.04@{DIGEST}
    strategy:
      matrix:
        compiler:
          - image: gcc:16.1.0@{DIGEST}
    steps:
      - uses: actions/checkout@{COMMIT} # v4.2.2
      - uses: ./.github/actions/local
      - uses: actions/setup-python@{COMMIT}
        with:
          python-version: ${{{{ env.PYTHON_VERSION }}}}
      - name: Outils
        run: |
          sh tools/ci/apt-install.sh cmake
          curl -fsSLo a.tar.gz https://exemple/a.tar.gz
          echo "x  a.tar.gz" | sha256sum --check
          pip install --require-hashes --only-binary=:all: -r tools/requirements/reuse.txt
      - run: cmake --version
"""


class WorkflowTest(unittest.TestCase):
    def errors(self, workflow: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "w.yml")
            path.write_text(workflow, encoding="utf-8")
            return check_pins.check_workflow(path, "w.yml")

    def assertError(self, workflow: str, fragment: str):
        errors = self.errors(workflow)
        self.assertTrue(any(fragment in e for e in errors), f"« {fragment} » absent de {errors}")

    def test_pinned_workflow(self):
        self.assertEqual(self.errors(PINNED_WORKFLOW), [])

    def test_action_by_tag(self):
        self.assertError(PINNED_WORKFLOW.replace(f"checkout@{COMMIT}", "checkout@v4"),
                         "« actions/checkout@v4 » non épinglée par commit complet")

    def test_action_by_short_commit(self):
        self.assertError(PINNED_WORKFLOW.replace(f"checkout@{COMMIT}", "checkout@11bd719"), "commit complet")

    def test_image_without_digest(self):
        self.assertError(PINNED_WORKFLOW.replace(f"ubuntu:26.04@{DIGEST}", "ubuntu:26.04"),
                         "image « ubuntu:26.04 » sans empreinte")
        self.assertError(PINNED_WORKFLOW.replace(f"gcc:16.1.0@{DIGEST}", "gcc:16.1.0"),
                         "image « gcc:16.1.0 » sans empreinte")

    def test_matrix_image_under_any_key(self):
        # Contre-épreuve de revue : clé de matrice autre que « image ».
        workflow = """jobs:
  build:
    container: ${{ matrix.os }}
    strategy:
      matrix:
        os: ["ubuntu:26.04"]
"""
        self.assertError(workflow, "image « ubuntu:26.04 » (matrice, ${{ matrix.os }}) sans empreinte")
        self.assertEqual(self.errors(workflow.replace('"ubuntu:26.04"', f'"ubuntu:26.04@{DIGEST}"')), [])

    def test_matrix_image_forms(self):
        block = f"""jobs:
  build:
    container: ${{{{ matrix.cible.os }}}}
    strategy:
      matrix:
        cible:
          - {{ name: a, os: ubuntu:26.04@{DIGEST} }}
          - {{ name: b, os: debian:13 }}
        os:
          - alpine:3
"""
        errors = self.errors(block)
        self.assertTrue(any("« debian:13 »" in e for e in errors), errors)
        self.assertTrue(any("« alpine:3 »" in e for e in errors), errors)
        self.assertFalse(any("ubuntu" in e for e in errors), errors)

    def test_matrix_image_without_values(self):
        self.assertError("jobs:\n  build:\n    container: ${{ matrix.absent }}\n", "introuvables dans la matrice")

    def test_unverifiable_image_expression(self):
        self.assertError("jobs:\n  build:\n    container: ${{ inputs.image }}\n", "expression non vérifiable")

    def test_docker_action_without_digest(self):
        self.assertError(PINNED_WORKFLOW.replace("uses: ./.github/actions/local", "uses: docker://alpine:3"),
                         "« docker://alpine:3 » sans empreinte")

    def test_python_version(self):
        self.assertError(PINNED_WORKFLOW.replace('PYTHON_VERSION: "3.12.14"', 'PYTHON_VERSION: "3.12"'),
                         "python-version « 3.12 » n'est pas une version complète")
        self.assertError(PINNED_WORKFLOW.replace("${{ env.PYTHON_VERSION }}", '"3.x"'), "« 3.x »")

    def test_direct_apt(self):
        for command in ("apt-get update", "apt-get install -y cmake", "apt install cmake",
                        "apt-get -q install cmake"):
            with self.subTest(command=command):
                self.assertError(PINNED_WORKFLOW.replace("sh tools/ci/apt-install.sh cmake", command),
                                 "appel direct à apt")

    def test_inline_run_is_checked(self):
        self.assertError(PINNED_WORKFLOW.replace("- run: cmake --version", "- run: apt-get install cmake"),
                         "appel direct à apt")

    def test_uncontrolled_installers(self):
        for command in ("pipx run reuse==6.2.0 lint", "uvx reuse", "npm install x", "npx x", "cargo install x"):
            with self.subTest(command=command):
                self.assertError(PINNED_WORKFLOW.replace("- run: cmake --version", f"- run: {command}"),
                                 "installe sans épinglage contrôlé")

    def test_pip_without_hashes(self):
        locked = "pip install --require-hashes --only-binary=:all: -r tools/requirements/reuse.txt"
        for command in ("pip install reuse==6.2.0", '"$RUNNER_TEMP/venv/bin/pip" install reuse==6.2.0',
                        "'venv/bin/pip3' install -r tools/requirements/reuse.txt", "python -m pip install reuse"):
            with self.subTest(command=command):
                self.assertError(PINNED_WORKFLOW.replace(locked, command), "pip install sans --require-hashes")
        quoted = '"$RUNNER_TEMP/venv/bin/pip" install --require-hashes -r tools/requirements/reuse.txt'
        self.assertEqual(self.errors(PINNED_WORKFLOW.replace(locked, quoted)), [])

    def test_download_without_checksum(self):
        self.assertError(PINNED_WORKFLOW.replace('echo "x  a.tar.gz" | sha256sum --check', "tar -xzf a.tar.gz"),
                         "téléchargement sans contrôle sha256sum")


class RequirementsTest(unittest.TestCase):
    def errors(self, text: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "r.txt")
            path.write_text(text, encoding="utf-8")
            return check_pins.check_requirements(path, "r.txt")

    def test_locked(self):
        self.assertEqual(self.errors(f"# verrou\nreuse==6.2.0 \\\n    {HASH} \\\n    {HASH}\n    # via -r\n"), [])

    def test_range_and_missing_hash(self):
        self.assertTrue(any("version exacte" in e for e in self.errors(f"reuse>=6 \\\n    {HASH}\n")))
        self.assertTrue(any("sans empreinte" in e for e in self.errors("reuse==6.2.0\n")))

    def test_empty_file(self):
        self.assertTrue(any("aucune entrée" in e for e in self.errors("# vide\n")))


class CMakeTest(unittest.TestCase):
    def errors(self, text: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "CMakeLists.txt")
            path.write_text(text, encoding="utf-8")
            return check_pins.check_cmake(path, "CMakeLists.txt")

    def test_pinned(self):
        self.assertEqual(self.errors(f"FetchContent_Declare(a GIT_REPOSITORY https://x GIT_TAG {COMMIT})\n"
                                     f"ExternalProject_Add(b URL https://x.tgz URL_HASH SHA256={'0' * 64})\n"), [])

    def test_tag_or_branch(self):
        self.assertTrue(any("GIT_TAG « v1.2 »" in e for e in self.errors(
            "FetchContent_Declare(a\n  GIT_REPOSITORY https://x\n  GIT_TAG v1.2\n)\n")))

    def test_url_without_hash(self):
        self.assertTrue(any("ni URL_HASH" in e for e in self.errors("FetchContent_Declare(a URL https://x.tgz)\n")))


class MainTest(unittest.TestCase):
    def test_missing_workflows(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stderr(io.StringIO()):
            self.assertEqual(check_pins.main(["check_pins.py", directory]), 1)

    def test_project_is_pinned(self):
        with redirect_stdout(io.StringIO()):
            self.assertEqual(check_pins.main(["check_pins.py"]), 0)


if __name__ == "__main__":
    unittest.main()
