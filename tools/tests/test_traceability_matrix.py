# SPDX-FileCopyrightText: 2026 Contributeurs du projet Clepsydre
# SPDX-License-Identifier: EUPL-1.2
"""Tests de tools/traceability_matrix.py ; lancer avec : python3 -m unittest discover -s tools/tests"""

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import traceability_matrix  # noqa: E402

REQUIREMENT = {"id": "SW-REQ-001", "title": "t", "text": "x", "source": "Mesure CTRL-001 (RISK-001)",
               "status": "draft"}
RISK = {"id": "RISK-001", "hazard": "h", "harm": "d", "status": "draft"}
CONTROL = {"id": "CTRL-001", "description": "c", "risks": ["RISK-001"], "requirements": ["SW-REQ-001"]}
PLANNED = {"id": "VER-001", "method": "test", "level": "unit", "requirements": ["SW-REQ-001"], "status": "planned"}


def entries(*verifications):
    return {"requirement": [REQUIREMENT], "risk": [RISK], "control": [CONTROL],
            "verification": list(verifications) or [PLANNED], "soup": []}


def row(matrix: str, identifier: str) -> str:
    return next(line for line in matrix.splitlines() if line.startswith(f"| {identifier} |"))


class RenderTest(unittest.TestCase):
    def test_planned_verification_is_not_a_result(self):
        matrix = traceability_matrix.render(entries())
        self.assertIn("pas qu'elle a réussi", matrix)
        line = row(matrix, "SW-REQ-001")
        self.assertIn("VER-001 (essai, unitaire, prévue)", line)
        self.assertIn("Non vérifiée (vérifications prévues)", line)
        self.assertNotIn("| Vérifiée |", matrix)

    def test_passed_verification_with_reference(self):
        passed = {**PLANNED, "status": "passed", "reference": "tests/a.cpp"}
        line = row(traceability_matrix.render(entries(passed)), "SW-REQ-001")
        self.assertIn("réussie, `tests/a.cpp`", line)
        self.assertTrue(line.endswith("| Vérifiée |"))

    def test_any_failure_marks_requirement_failed(self):
        passed = {**PLANNED, "status": "passed", "reference": "a"}
        failed = {**PLANNED, "id": "VER-002", "status": "failed", "reference": "b"}
        self.assertIn("**Échec**", row(traceability_matrix.render(entries(passed, failed)), "SW-REQ-001"))

    def test_partial_verification(self):
        passed = {**PLANNED, "status": "passed", "reference": "a"}
        other = {**PLANNED, "id": "VER-002"}
        self.assertIn("Partiellement vérifiée", row(traceability_matrix.render(entries(passed, other)), "SW-REQ-001"))

    def test_risk_chain(self):
        line = row(traceability_matrix.render(entries()), "RISK-001")
        self.assertEqual(line, "| RISK-001 | draft | CTRL-001 | SW-REQ-001 | non vérifiée (vérifications prévues) : 1 |")

    def test_revision_in_header(self):
        self.assertIn("`abc123`", traceability_matrix.render(entries(), "abc123"))


class MainTest(unittest.TestCase):
    def test_inconsistent_registry_produces_no_matrix(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "controls.toml").write_text(
                '[[control]]\nid = "CTRL-001"\ndescription = "c"\nrisks = ["RISK-404"]\nrequirements = ["SW-REQ-404"]\n',
                encoding="utf-8")
            output = Path(directory, "matrix.md")
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                code = traceability_matrix.main(["traceability_matrix.py", "--registry", directory, "-o", str(output)])
            self.assertEqual(code, 1)
            self.assertFalse(output.exists())
            self.assertIn("« RISK-404 » inexistante", stderr.getvalue())

    def test_missing_registry_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory, "matrix.md")
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                code = traceability_matrix.main(["traceability_matrix.py", "--registry", str(Path(directory, "absent")),
                                                 "-o", str(output)])
            self.assertEqual(code, 1)
            self.assertFalse(output.exists())
            self.assertIn("Registre introuvable", stderr.getvalue())

    def test_project_matrix(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory, "matrix.md")
            self.assertEqual(traceability_matrix.main(["traceability_matrix.py", "-o", str(output)]), 0)
            matrix = output.read_text(encoding="utf-8")
        self.assertIn("## Risques, mesures et exigences", matrix)
        self.assertIn("| RISK-001 |", matrix)


if __name__ == "__main__":
    unittest.main()
