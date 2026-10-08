#!/usr/bin/env python3
"""End-to-end tests for the external ILBM qualification CLI."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "qualify_external_ilbm.py"


def iff(form_type=b"ILBM"):
    bmhd = bytes([0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 10, 10, 0, 1, 0, 1])
    chunks = b"BMHD" + (20).to_bytes(4, "big") + bmhd + b"BODY" + (2).to_bytes(4, "big") + b"\x80\0"
    payload = form_type + chunks
    return b"FORM" + len(payload).to_bytes(4, "big") + payload


class QualificationTests(unittest.TestCase):
    def run_qualification(self, sample, cli):
        p = subprocess.run([sys.executable, str(SCRIPT), str(sample), "--cli", str(cli)],
                           capture_output=True, text=True, check=False)
        self.assertEqual(p.stderr, "")
        return p.returncode, json.loads(p.stdout)

    def test_valid_with_real_cli(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "valid.iff"
            sample.write_bytes(iff())
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["cli_exit"], 0)
            self.assertEqual(report["planes"], 1)

    def test_wrong_form_rejected_before_cli(self):
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "wrong.iff"
            sample.write_bytes(iff(b"TEST"))
            rc, report = self.run_qualification(sample, "missing-cli")
            self.assertEqual(rc, 1)
            self.assertEqual(report["error"], "not FORM ILBM")

    def test_missing_cli_reported(self):
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "valid.iff"
            sample.write_bytes(iff())
            rc, report = self.run_qualification(sample, pathlib.Path(d) / "missing-cli")
            self.assertEqual(rc, 1)
            self.assertFalse(report["qualified"])
            self.assertIn("error", report)


if __name__ == "__main__":
    unittest.main()
