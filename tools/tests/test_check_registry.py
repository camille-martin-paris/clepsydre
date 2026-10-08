# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Tests de tools/check_registry.py ; lancer avec : python3 -m unittest discover tools/tests"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_registry  # noqa: E402

REQUIREMENT = {"id": "SW-REQ-001", "title": "t", "text": "x", "source": "s", "status": "approved"}
RISK = {"id": "RISK-001", "hazard": "h", "harm": "d", "status": "controlled"}
CONTROL = {"id": "CTRL-001", "description": "c", "risks": ["RISK-001"], "requirements": ["SW-REQ-001"]}
VERIFICATION = {"id": "VER-001", "method": "test", "requirements": ["SW-REQ-001"], "status": "planned"}
SOUP = {"id": "SOUP-001", "name": "lib", "version": "1.2.3", "usage": "u", "requirements": ["SW-REQ-001"],
        "known_anomalies": "aucune connue"}


def registry(**overrides):
    entries = {
        "requirement": [REQUIREMENT],
        "risk": [RISK],
        "control": [CONTROL],
        "verification": [VERIFICATION],
        "soup": [SOUP],
    }
    entries.update(overrides)
    return entries


class CheckRegistryTest(unittest.TestCase):
    def assertError(self, entries, fragment):
        errors = check_registry.check(entries)
        self.assertTrue(any(fragment in e for e in errors), f"« {fragment} » absent de {errors}")

    def test_complete_chain_is_consistent(self):
        self.assertEqual(check_registry.check(registry()), [])

    def test_empty_registry_is_consistent(self):
        empty = {key: [] for key in ("requirement", "risk", "control", "verification", "soup")}
        self.assertEqual(check_registry.check(empty), [])

    def test_invalid_identifier(self):
        self.assertError(registry(requirement=[{**REQUIREMENT, "id": "REQ-1"}]), "identifiant invalide")

    def test_duplicate_identifier(self):
        self.assertError(registry(risk=[RISK, RISK]), "en double")

    def test_missing_field(self):
        incomplete = {k: v for k, v in RISK.items() if k != "harm"}
        self.assertError(registry(risk=[incomplete]), "« harm » absent")

    def test_unknown_status(self):
        self.assertError(registry(verification=[{**VERIFICATION, "status": "ok"}]), "statut « ok » inconnu")

    def test_dangling_reference(self):
        self.assertError(registry(control=[{**CONTROL, "risks": ["RISK-999"]}]), "« RISK-999 » inexistante")

    def test_control_without_requirement(self):
        self.assertError(registry(control=[{**CONTROL, "requirements": []}]), "ne doit pas être vide")

    def test_controlled_risk_without_control(self):
        self.assertError(registry(control=[], risk=[RISK]), "sans mesure de maîtrise")

    def test_approved_requirement_without_verification(self):
        self.assertError(registry(verification=[]), "sans vérification")

    def test_draft_requirement_may_lack_verification(self):
        entries = registry(requirement=[{**REQUIREMENT, "status": "draft"}], verification=[])
        self.assertEqual(check_registry.check(entries), [])

    def test_unknown_verification_method(self):
        self.assertError(registry(verification=[{**VERIFICATION, "method": "intuition"}]), "méthode")

    def test_unpinned_soup_version(self):
        for version in ("^1.2", ">=1.0", "latest", "main", "1.*"):
            with self.subTest(version=version):
                self.assertError(registry(soup=[{**SOUP, "version": version}]), "non épinglée")


class MainTest(unittest.TestCase):
    def run_main(self, files):
        with tempfile.TemporaryDirectory() as directory:
            for name, content in files.items():
                Path(directory, name).write_text(content, encoding="utf-8")
            return check_registry.main(["check_registry.py", directory])

    def test_valid_files(self):
        files = {
            "requirements.toml": '[[requirement]]\nid = "SYS-REQ-001"\ntitle = "t"\ntext = "x"\n'
                                 'source = "s"\nstatus = "draft"\n',
        }
        self.assertEqual(self.run_main(files), 0)

    def test_invalid_toml(self):
        self.assertEqual(self.run_main({"risks.toml": "[[risk]\n"}), 1)

    def test_project_registry_is_consistent(self):
        self.assertEqual(check_registry.main(["check_registry.py"]), 0)


if __name__ == "__main__":
    unittest.main()
