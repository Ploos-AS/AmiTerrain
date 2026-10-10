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
            self.assertEqual(report["heightmap_status"], "matched")
            self.assertEqual(report["cli_exit"], 0)
            self.assertEqual(report["planes"], 1)

    def test_one_wrong_height_sample_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            sample = root / "valid.iff"
            sample.write_bytes(iff())
            fake = root / "bad-cli"
            fake.write_text("#!/usr/bin/env python3\n"
                            "import pathlib, sys\n"
                            "if sys.argv[1] == 'validate': sys.exit(0)\n"
                            "if sys.argv[1] == 'convert':\n"
                            "    pathlib.Path(sys.argv[3]).write_bytes(bytes([0xff, 0xfe]))\n"
                            "    sys.exit(0)\n"
                            "sys.exit(1)\n")
            fake.chmod(0o755)
            rc, report = self.run_qualification(sample, fake)
            self.assertEqual(rc, 1)
            self.assertFalse(report["qualified"])
            self.assertTrue(report["heightmap_compared"])
            self.assertFalse(report["heightmap_matches_reference"])
            self.assertEqual(report["heightmap_actual_byte_count"], 2)
            self.assertEqual(report["heightmap_status"], "mismatch")

    def test_native_output_truncated_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            sample = root / "valid.iff"
            sample.write_bytes(iff())
            fake = root / "truncated-cli"
            fake.write_text("#!/usr/bin/env python3\n"
                            "import pathlib, sys\n"
                            "if sys.argv[1] == 'validate': sys.exit(0)\n"
                            "if sys.argv[1] == 'convert':\n"
                            "    pathlib.Path(sys.argv[3]).write_bytes(bytes([0xff]))\n"
                            "    sys.exit(0)\n"
                            "sys.exit(1)\n")
            fake.chmod(0o755)
            rc, report = self.run_qualification(sample, fake)
            self.assertEqual(rc, 1)
            self.assertFalse(report["qualified"])
            self.assertTrue(report["heightmap_compared"])
            self.assertFalse(report["heightmap_matches_reference"])
            self.assertEqual(report["heightmap_actual_byte_count"], 1)
            self.assertEqual(report["heightmap_status"], "mismatch")

    def test_native_convert_failure_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            sample = root / "valid.iff"
            sample.write_bytes(iff())
            fake = root / "failing-cli"
            fake.write_text("#!/usr/bin/env python3\n"
                            "import sys\n"
                            "if sys.argv[1] == 'validate': sys.exit(0)\n"
                            "if sys.argv[1] == 'convert':\n"
                            "    print('native conversion failed', file=sys.stderr)\n"
                            "    sys.exit(1)\n"
                            "sys.exit(1)\n")
            fake.chmod(0o755)
            rc, report = self.run_qualification(sample, fake)
            self.assertEqual(rc, 1)
            self.assertFalse(report["qualified"])
            self.assertFalse(report["heightmap_compared"])
            self.assertIn("native conversion failed", report["heightmap_error"])
            self.assertEqual(report["heightmap_status"], "conversion_failed")

    def test_native_convert_success_without_output_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            sample = root / "valid.iff"
            sample.write_bytes(iff())
            fake = root / "silent-cli"
            fake.write_text("#!/usr/bin/env python3\n"
                            "import sys\n"
                            "if sys.argv[1] in ('validate', 'convert'): sys.exit(0)\n"
                            "sys.exit(1)\n")
            fake.chmod(0o755)
            rc, report = self.run_qualification(sample, fake)
            self.assertEqual(rc, 1)
            self.assertFalse(report["qualified"])
            self.assertFalse(report["heightmap_compared"])
            self.assertIn("heightmap_error", report)
            self.assertEqual(report["heightmap_status"], "output_unavailable")

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
