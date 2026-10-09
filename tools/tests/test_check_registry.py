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

    def test_mistyped_text_fields_are_rejected(self):
        # Contre-exemple de revue : id valide mais champs texte remplacés par des listes vides.
        entry = {"id": "SW-REQ-001", "title": [], "text": [], "source": [], "status": "draft"}
        errors = check_registry.check(registry(requirement=[entry], verification=[], control=[], soup=[]))
        for field in ("title", "text", "source"):
            self.assertIn(f"requirements.toml: SW-REQ-001: champ « {field} » doit être une chaîne, pas list", errors)

    def test_blank_text_field(self):
        self.assertError(registry(risk=[{**RISK, "harm": "  "}]), "champ « harm » ne doit pas être vide")

    def test_reference_list_must_hold_strings(self):
        self.assertError(registry(control=[{**CONTROL, "risks": [1]}]), "« risks » doit être une liste de chaînes")
        self.assertError(registry(control=[{**CONTROL, "risks": "RISK-001"}]), "« risks » doit être une liste de chaînes")

    def test_non_string_identifier(self):
        self.assertError(registry(risk=[{**RISK, "id": 1}]), "identifiant invalide")

    def test_unknown_field(self):
        self.assertError(registry(risk=[{**RISK, "severity": "x"}]), "champ « severity » inconnu")

    def test_optional_reference(self):
        self.assertEqual(check_registry.check(registry(verification=[{**VERIFICATION, "reference": "tests/a"}])), [])
        self.assertError(registry(verification=[{**VERIFICATION, "reference": 3}]), "« reference » doit être une chaîne")

    def test_soup_version_must_be_string(self):
        self.assertError(registry(soup=[{**SOUP, "version": 1.2}]), "« version » doit être une chaîne")

    def test_unpinned_soup_version(self):
        for version in ("^1.2", ">=1.0", "latest", "main", "1.*", "release", "stable"):
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

    def test_unknown_root_table_is_rejected(self):
        # Contre-exemple de revue : « [[requirements]] » au lieu de « [[requirement]] ».
        files = {"requirements.toml": '[[requirements]]\nid = "SW-REQ-001"\n'}
        self.assertEqual(self.run_main(files), 1)

    def test_load_reports_unknown_root_table(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "requirements.toml").write_text('[[requirements]]\nid = "SW-REQ-001"\n', encoding="utf-8")
            entries, errors = check_registry.load(Path(directory))
        self.assertEqual(entries["requirement"], [])
        self.assertEqual(errors, ["requirements.toml: clé racine « requirements » inconnue, "
                                  "seule « [[requirement]] » est admise"])

    def test_root_key_must_be_array_of_tables(self):
        self.assertEqual(self.run_main({"risks.toml": 'risk = "RISK-001"\n'}), 1)
        self.assertEqual(self.run_main({"risks.toml": "risk = [1, 2]\n"}), 1)

    def test_comment_only_file_is_accepted(self):
        self.assertEqual(self.run_main({"risks.toml": "# aucun risque\n"}), 0)

    def test_invalid_toml(self):
        self.assertEqual(self.run_main({"risks.toml": "[[risk]\n"}), 1)

    def test_project_registry_is_consistent(self):
        self.assertEqual(check_registry.main(["check_registry.py"]), 0)


if __name__ == "__main__":
    unittest.main()
