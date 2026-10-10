#!/usr/bin/env python3
"""Tests for the orchestration and evidence produced on download failure."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run_ilbm_qualification


class QualificationRunnerTests(unittest.TestCase):
    def test_download_failure_produces_valid_json_report(self):
        with tempfile.TemporaryDirectory() as temp:
            output = pathlib.Path(temp) / "evidence"
            with patch.object(sys, "argv", ["run_ilbm_qualification.py", str(output)]):
                with patch.object(run_ilbm_qualification.subprocess, "run",
                                  return_value=subprocess.CompletedProcess([], 1)):
                    rc = run_ilbm_qualification.main()
            self.assertEqual(rc, 1)
            report = json.loads((output / "amiga_lagoon.qualification.json").read_text())
            self.assertFalse(report["qualified"])
            self.assertEqual(report["stage"], "download")
            self.assertIn("error", report)
            self.assertFalse((output / "amiga_lagoon.iff").exists())


if __name__ == "__main__":
    unittest.main()
