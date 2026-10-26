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

    def test_byterun1_real_cli_matches_reference(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "compressed.iff"
            raw = bytearray(iff())
            # BODY consists of one 16-bit plane row. Encode two zero bytes as
            # a ByteRun1 repeat, and enable compression in BMHD.
            raw[30] = 1
            raw[44:48] = (2).to_bytes(4, "big")
            raw[48:] = bytes([0xff, 0])
            raw[4:8] = (len(raw) - 8).to_bytes(4, "big")
            sample.write_bytes(raw)
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_byterun1_multiple_planes_and_rows(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "multi-plane.iff"
            raw = bytearray(iff())
            # Two rows, two planes, one 16-bit word per plane row.
            raw[22:24] = (2).to_bytes(2, "big")
            raw[28] = 2
            raw[30] = 1
            # Each plane row is independently encoded as two literal bytes.
            encoded = bytes([1, 0x80, 0, 1, 0x40, 0,
                             1, 0x20, 0, 1, 0x10, 0])
            raw[44:48] = len(encoded).to_bytes(4, "big")
            raw[48:] = encoded
            raw[4:8] = (len(raw) - 8).to_bytes(4, "big")
            sample.write_bytes(raw)
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["reference_heightmap_sample_count"], 2)
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_cli_rejects_cross_plane_byterun1_packet(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "cross-plane.iff"
            raw = bytearray(iff())
            raw[28] = 2  # two planes, two decoded bytes per plane
            raw[30] = 1  # ByteRun1
            raw[48:] = bytes([0xfd, 0])  # one packet expands to four bytes
            raw[4:8] = (len(raw) - 8).to_bytes(4, "big")
            sample.write_bytes(raw)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0,
                                "native decoder accepted a cross-plane ByteRun1 packet")

    def test_native_cli_rejects_malformed_byterun1_variants(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        variants = {
            "truncated-literal": bytes([1, 0x80]),
            "truncated-repeat": bytes([0xff]),
            "trailing-packet": bytes([0xff, 0, 0x00]),
        }
        with tempfile.TemporaryDirectory() as d:
            for name, body in variants.items():
                with self.subTest(name=name):
                    sample = pathlib.Path(d) / (name + ".iff")
                    raw = bytearray(iff())
                    raw[30] = 1  # ByteRun1
                    raw[44:48] = len(body).to_bytes(4, "big")
                    raw[48:] = body + (bytes([0]) if len(body) & 1 else b"")
                    raw[4:8] = (len(raw) - 8).to_bytes(4, "big")
                    sample.write_bytes(raw)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_form_length_mismatch(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        variants = {
            "trailing-file-byte": valid + bytes([0]),
            "truncated-file": valid[:-1],
            "oversized-form": valid[:4] + (len(valid) + 5).to_bytes(4, "big") + valid[8:],
        }
        with tempfile.TemporaryDirectory() as d:
            for name, data in variants.items():
                with self.subTest(name=name):
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(data)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_noncanonical_chunk_sizes(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        base = iff()
        def chunk(tag, payload):
            return tag + len(payload).to_bytes(4, "big") + payload + (bytes([0]) if len(payload) & 1 else b"")
        bmhd = base[20:40]
        body = base[48:50]
        variants = {
            "bmhd-oversized": chunk(b"BMHD", bmhd + bytes([0, 0])) + chunk(b"BODY", body),
            "bmhd-undersized": chunk(b"BMHD", bmhd[:-2]) + chunk(b"BODY", body),
            "camg-oversized": chunk(b"BMHD", bmhd) + chunk(b"CAMG", bytes(6)) + chunk(b"BODY", body),
            "camg-undersized": chunk(b"BMHD", bmhd) + chunk(b"CAMG", bytes(2)) + chunk(b"BODY", body),
        }
        with tempfile.TemporaryDirectory() as d:
            for name, chunks in variants.items():
                with self.subTest(name=name):
                    payload = b"ILBM" + chunks
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_incomplete_chunk_header(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        # A FORM with a complete image followed by a partial 4-byte chunk ID.
        payload = valid[8:] + b"JUNK"
        malformed = b"FORM" + len(payload).to_bytes(4, "big") + payload
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "partial-chunk.iff"
            sample.write_bytes(malformed)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0,
                                "native decoder accepted incomplete IFF chunk header")

    def test_native_cli_accepts_unknown_padded_chunk(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        # Unknown odd-sized chunks require one padding byte, which is not
        # counted in the chunk payload length.
        unknown = b"JUNK" + (3).to_bytes(4, "big") + b"abc" + bytes([0])
        payload = valid[8:40] + unknown + valid[40:]
        sample_bytes = b"FORM" + len(payload).to_bytes(4, "big") + payload
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "unknown-padded.iff"
            sample.write_bytes(sample_bytes)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_native_cli_accepts_body_before_bmhd(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd_chunk = valid[12:40]
        body_chunk = valid[40:]
        payload = b"ILBM" + body_chunk + bmhd_chunk
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "body-first.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_native_cli_rejects_duplicate_essential_chunks(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd_chunk = valid[12:40]
        body_chunk = valid[40:]
        variants = {
            "duplicate-bmhd": bmhd_chunk + bmhd_chunk + body_chunk,
            "duplicate-body": bmhd_chunk + body_chunk + body_chunk,
        }
        with tempfile.TemporaryDirectory() as d:
            for name, chunks in variants.items():
                with self.subTest(name=name):
                    payload = b"ILBM" + chunks
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_accepts_unknown_chunk_after_body(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        unknown = b"JUNK" + (3).to_bytes(4, "big") + b"xyz" + bytes([0])
        payload = valid[8:] + unknown
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "unknown-after-body.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_native_cli_rejects_missing_and_duplicate_chunks(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd_chunk = valid[12:40]
        body_chunk = valid[40:]
        camg_chunk = b"CAMG" + (4).to_bytes(4, "big") + bytes(4)
        variants = {
            "missing-bmhd": body_chunk,
            "missing-body": bmhd_chunk,
            "duplicate-camg": bmhd_chunk + camg_chunk + camg_chunk + body_chunk,
        }
        with tempfile.TemporaryDirectory() as d:
            for name, chunks in variants.items():
                with self.subTest(name=name):
                    payload = b"ILBM" + chunks
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_invalid_bmhd_fields(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        # Offsets refer to BMHD payload within the canonical one-pixel fixture.
        variants = {
            "zero-width": (21, 0),
            "zero-height": (23, 0),
            "zero-planes": (28, 0),
            "nine-planes": (28, 9),
            "unsupported-mask": (29, 2),
            "unsupported-compression": (30, 2),
        }
        with tempfile.TemporaryDirectory() as d:
            for name, (offset, value) in variants.items():
                with self.subTest(name=name):
                    raw = bytearray(iff())
                    raw[offset] = value
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(raw)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_uncompressed_body_size_mismatch(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd = valid[12:40]
        variants = {
            "empty-body": b"",
            "oversized-body": bytes([0x80, 0, 0, 0]),
        }
        with tempfile.TemporaryDirectory() as d:
            for name, body in variants.items():
                with self.subTest(name=name):
                    body_chunk = (b"BODY" + len(body).to_bytes(4, "big") +
                                  body + (bytes([0]) if len(body) & 1 else b""))
                    payload = b"ILBM" + bmhd + body_chunk
                    sample = pathlib.Path(d) / (name + ".iff")
                    sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
                    result = subprocess.run([str(cli), "validate", str(sample)],
                                            capture_output=True, text=True, check=False)
                    self.assertNotEqual(result.returncode, 0,
                                        "native decoder accepted " + name)

    def test_native_cli_rejects_byterun1_below_minimum_size(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd = bytearray(valid[20:40])
        bmhd[1] = 2  # width = 2
        bmhd[3] = 2  # height = 2
        bmhd[8] = 2  # two bitplanes
        bmhd[10] = 1  # ByteRun1 compression
        # Four plane rows require at least eight encoded bytes.
        body = bytes([0, 0, 0, 0, 0, 0])
        chunks = (b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                  b"BODY" + len(body).to_bytes(4, "big") + body)
        payload = b"ILBM" + chunks
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-too-short.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0,
                                "native decoder accepted undersized ByteRun1 BODY")

    def test_native_cli_accepts_byterun1_nops(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        valid = iff()
        bmhd = bytearray(valid[20:40])
        bmhd[10] = 1  # ByteRun1 compression
        # One 16-bit row: NOP, literal two bytes, NOP.
        body = bytes([0x80, 0x01, 0x80, 0x00, 0x80])
        body_chunk = (b"BODY" + len(body).to_bytes(4, "big") +
                      body + bytes([0]))  # IFF pad is outside BODY length
        payload = b"ILBM" + valid[12:20] + bmhd + body_chunk
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-nops.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            result = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            from tools.reference_ilbm import decode
            width, height, pixels = decode(sample.read_bytes())
            self.assertEqual((width, height, pixels), (1, 1, bytes([1])))

    def test_native_and_reference_match_multiplane_multirow_nops(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[3] = 2
        bmhd[8] = 2
        bmhd[10] = 1
        body = bytes([0x80, 0xff, 0x80,
                      0x80, 0xff, 0x00,
                      0x80, 0xff, 0x00,
                      0x80, 0xff, 0x80, 0x80])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body + bytes([0]))
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "multiplane-nops.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            self.assertEqual(decode(sample.read_bytes()), (1, 2, bytes([1, 2])))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_mixed_byterun1_packets(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[3] = 2
        bmhd[8] = 2
        bmhd[10] = 1
        body = bytes([
            0x80, 0x01, 0x80, 0x00,
            0x80, 0xff, 0x00,
            0x00, 0x40, 0x00, 0x00,
            0x80, 0xff, 0x80, 0x80,
        ])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body + bytes([0]))
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "mixed-byterun1.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            self.assertEqual(decode(sample.read_bytes()), (1, 2, bytes([1, 2])))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_byterun1_mask_plane(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[9] = 1  # one additional mask plane
        bmhd[10] = 1  # ByteRun1
        # Image plane yields index 1; mask plane is deliberately all zero.
        body = bytes([0xff, 0x80, 0x80, 0xff, 0x00])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body + bytes([0]))
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-mask.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            self.assertEqual(decode(sample.read_bytes()), (1, 1, bytes([1])))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_multirow_mask_patterns(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[3] = 2
        bmhd[9] = 1
        bmhd[10] = 1
        # Image indices 1,0; mask bits 0,1. Each plane row is separate.
        body = bytes([0xff, 0x80, 0xff, 0x00,
                      0xff, 0x00, 0xff, 0x80])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body)
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-multirow-mask.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            self.assertEqual(decode(sample.read_bytes()), (1, 2, bytes([1, 0])))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_byte_boundary_with_mask(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[1] = 9   # width crosses the first byte boundary
        bmhd[9] = 1   # separate mask plane
        bmhd[10] = 1  # ByteRun1
        # Image bits: pixel 0 and pixel 8 set. Mask bits differ.
        # Literal packets each decode a two-byte plane row.
        body = bytes([0x01, 0x80, 0x80,
                      0x80, 0x01, 0x7f, 0x00])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body + bytes([0]))
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-byte-boundary-mask.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            self.assertEqual(decode(sample.read_bytes()),
                             (9, 1, bytes([1, 0, 0, 0, 0, 0, 0, 0, 1])))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_17_pixel_word_alignment(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[1] = 17  # 17 pixels require four bytes per plane row
        bmhd[9] = 1   # mask plane must use the same aligned stride
        bmhd[10] = 1  # ByteRun1
        # Set image pixels 0, 8 and 16. Fourth byte is alignment padding.
        image = bytes([0x80, 0x80, 0x80, 0x00])
        mask = bytes([0x00, 0x00, 0x00, 0x00])
        body = bytes([0x03]) + image + bytes([0x80, 0xfd, 0x00])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body)
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-word-alignment.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            expected = bytes(1 if i in (0, 8, 16) else 0 for i in range(17))
            self.assertEqual(decode(sample.read_bytes()), (17, 1, expected))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_native_and_reference_match_multirow_multiplane_alignment(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[1] = 17
        bmhd[3] = 2
        bmhd[8] = 2
        bmhd[9] = 1
        bmhd[10] = 1
        # Plane rows are interleaved: row 0 (plane 0, plane 1, mask),
        # then row 1 in the same order. Each decodes to four bytes.
        plane_rows = [
            bytes([0x80, 0x00, 0x80, 0x00]),  # row 0, bit 0 at x=0,16
            bytes([0x00, 0x80, 0x00, 0x00]),  # row 0, bit 1 at x=8
            bytes([0xff, 0x00, 0x00, 0x00]),  # row 0, mask
            bytes([0x00, 0x80, 0x00, 0x00]),  # row 1, bit 0 at x=8
            bytes([0x80, 0x00, 0x80, 0x00]),  # row 1, bit 1 at x=0,16
            bytes([0x00, 0x00, 0x00, 0x00]),  # row 1, mask
        ]
        body = b"".join(bytes([0x03]) + row for row in plane_rows)
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body)
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-multiplane-aligned.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            row0 = bytes(1 if i in (0, 16) else 2 if i == 8 else 0
                         for i in range(17))
            row1 = bytes(2 if i in (0, 16) else 1 if i == 8 else 0
                         for i in range(17))
            self.assertEqual(decode(sample.read_bytes()), (17, 2, row0 + row1))
            rc, report = self.run_qualification(sample, cli)
            self.assertEqual(rc, 0, report)
            self.assertTrue(report["qualified"])
            self.assertEqual(report["heightmap_status"], "matched")

    def test_byterun1_packet_cannot_cross_aligned_plane_row(self):
        cli = ROOT / "amiterrain"
        if not cli.is_file():
            self.skipTest("build amiterrain first")
        from tools.reference_ilbm import decode
        bmhd = bytearray(iff()[20:40])
        bmhd[1] = 17
        bmhd[8] = 2
        bmhd[10] = 1
        # Each aligned plane row is four bytes. This repeat packet
        # requests five bytes and must not spill into the next plane.
        body = bytes([0xfc, 0x80, 0x02, 0x00, 0x00, 0x00])
        payload = (b"ILBM" + b"BMHD" + (20).to_bytes(4, "big") + bmhd +
                   b"BODY" + len(body).to_bytes(4, "big") + body)
        with tempfile.TemporaryDirectory() as d:
            sample = pathlib.Path(d) / "byterun1-cross-plane.iff"
            sample.write_bytes(b"FORM" + len(payload).to_bytes(4, "big") + payload)
            with self.assertRaises(ValueError):
                decode(sample.read_bytes())
            native = subprocess.run([str(cli), "validate", str(sample)],
                                    capture_output=True, text=True, check=False)
            self.assertNotEqual(native.returncode, 0)

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
