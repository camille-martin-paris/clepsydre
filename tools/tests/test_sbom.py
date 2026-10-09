# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Tests de tools/sbom.py ; lancer avec : python3 -m unittest discover -s tools/tests"""

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import sbom  # noqa: E402

SOUP = {"id": "SOUP-001", "name": "lib", "version": "1.2.3", "supplier": "Projet lib", "license": "MIT OR Apache-2.0",
        "usage": "u", "requirements": ["SW-REQ-001", "SW-REQ-002"], "known_anomalies": "aucune"}
COMMIT = "a" * 40
TIMESTAMP = "2026-10-09T12:00:00+00:00"


class RenderTest(unittest.TestCase):
    def test_product_without_soup(self):
        bom = sbom.render([], "v0.1.0", COMMIT, TIMESTAMP)
        self.assertEqual((bom["bomFormat"], bom["specVersion"]), ("CycloneDX", "1.6"))
        component = bom["metadata"]["component"]
        self.assertEqual((component["type"], component["version"]), ("firmware", "v0.1.0"))
        self.assertEqual(component["licenses"], [{"license": {"id": "EUPL-1.2"}}])
        self.assertEqual(bom["metadata"]["timestamp"], TIMESTAMP)
        self.assertEqual(bom["components"], [])
        self.assertEqual(bom["dependencies"], [{"ref": "clepsydre", "dependsOn": []}])

    def test_soup_component(self):
        bom = sbom.render([{**SOUP, "purl": "pkg:github/x/lib@1.2.3"}], "v0.1.0", COMMIT, TIMESTAMP)
        (component,) = bom["components"]
        self.assertEqual(component["bom-ref"], "SOUP-001")
        self.assertEqual(component["version"], "1.2.3")
        self.assertEqual(component["supplier"], {"name": "Projet lib"})
        self.assertEqual(component["licenses"], [{"expression": "MIT OR Apache-2.0"}])
        self.assertEqual(component["purl"], "pkg:github/x/lib@1.2.3")
        self.assertIn({"ref": "clepsydre", "dependsOn": ["SOUP-001"]}, bom["dependencies"])

    def test_purl_is_optional(self):
        self.assertNotIn("purl", sbom.render([SOUP], "v", COMMIT, TIMESTAMP)["components"][0])

    def test_reproducible(self):
        first = sbom.render([SOUP], "v0.1.0", COMMIT, TIMESTAMP)
        self.assertEqual(first, sbom.render([SOUP], "v0.1.0", COMMIT, TIMESTAMP))
        self.assertNotEqual(first["serialNumber"], sbom.render([SOUP], "v0.1.1", COMMIT, TIMESTAMP)["serialNumber"])


class MainTest(unittest.TestCase):
    def run_main(self, directory: str, *extra: str) -> tuple[int, str]:
        output = Path(directory, "bom.json")
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            code = sbom.main(["sbom.py", "--registry", directory, "--version", "v0.1.0", "--commit", COMMIT,
                              "--timestamp", TIMESTAMP, "-o", str(output), *extra])
        return code, stderr.getvalue()

    def test_writes_bom(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "soup.toml").write_text(
                '[[soup]]\nid = "SOUP-001"\nname = "lib"\nversion = "1.2.3"\nsupplier = "s"\nlicense = "MIT"\n'
                'usage = "u"\nrequirements = ["SW-REQ-001"]\nknown_anomalies = "aucune"\n', encoding="utf-8")
            Path(directory, "requirements.toml").write_text(
                '[[requirement]]\nid = "SW-REQ-001"\ntitle = "t"\ntext = "x"\nsource = "s"\nstatus = "draft"\n',
                encoding="utf-8")
            Path(directory, "verifications.toml").write_text(
                '[[verification]]\nid = "VER-001"\nmethod = "test"\nlevel = "unit"\nrequirements = ["SW-REQ-001"]\n'
                'status = "planned"\n', encoding="utf-8")
            code, _ = self.run_main(directory)
            self.assertEqual(code, 0)
            bom = json.loads(Path(directory, "bom.json").read_text(encoding="utf-8"))
        self.assertEqual([c["name"] for c in bom["components"]], ["lib"])

    def test_inconsistent_registry_produces_no_bom(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "soup.toml").write_text(
                '[[soup]]\nid = "SOUP-001"\nname = "lib"\nversion = "latest"\nsupplier = "s"\nlicense = "MIT"\n'
                'usage = "u"\nrequirements = ["SW-REQ-404"]\nknown_anomalies = "aucune"\n', encoding="utf-8")
            code, stderr = self.run_main(directory)
            self.assertEqual(code, 1)
            self.assertFalse(Path(directory, "bom.json").exists())
            self.assertIn("non épinglée", stderr)

    def test_missing_registry(self):
        with tempfile.TemporaryDirectory() as directory:
            code, stderr = self.run_main(str(Path(directory, "absent")))
            self.assertEqual(code, 1)
            self.assertIn("Registre introuvable", stderr)


if __name__ == "__main__":
    unittest.main()
