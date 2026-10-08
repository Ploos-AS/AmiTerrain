#!/usr/bin/env python3
"""Self-contained regression tests for the external ILBM qualification inspector."""
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("qualify_ilbm", ROOT / "tools" / "qualify_ilbm.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def chunk(name, payload):
    return name + len(payload).to_bytes(4, "big") + payload + (b"\0" if len(payload) & 1 else b"")


def form(*chunks):
    payload = b"ILBM" + b"".join(chunks)
    return b"FORM" + len(payload).to_bytes(4, "big") + payload


class InspectorTests(unittest.TestCase):
    def inspect_bytes(self, data):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / "sample.iff"
            path.write_bytes(data)
            return module.inspect(path)

    def test_valid_metadata_and_padding(self):
        bmhd = bytes([0, 17, 0, 2, 0, 0, 0, 0, 8, 0, 1, 0, 0, 0, 10, 10, 0, 17, 0, 2])
        data = form(chunk(b"BMHD", bmhd), chunk(b"NOTE", b"x"),
                    chunk(b"CAMG", bytes([0, 0, 0, 0])), chunk(b"BODY", b"\0\0"))
        result = self.inspect_bytes(data)
        self.assertEqual((result["width"], result["height"], result["planes"]), (17, 2, 8))
        self.assertEqual(result["compression"], 1)
        self.assertEqual(result["camg"], 0)
        self.assertEqual([c["type"] for c in result["chunks"]], ["BMHD", "NOTE", "CAMG", "BODY"])
        self.assertEqual(len(result["sha256"]), 64)

    def test_form_length_mismatch(self):
        with self.assertRaisesRegex(ValueError, "FORM length"):
            self.inspect_bytes(form(chunk(b"BODY", b"\0\0")) + b"x")

    def test_chunk_outside_form(self):
        data = b"FORM" + (14).to_bytes(4, "big") + b"ILBM" + b"BODY" + (10).to_bytes(4, "big") + b"\0\0"
        with self.assertRaisesRegex(ValueError, "chunk extends"):
            self.inspect_bytes(data)

    def test_incomplete_chunk_header(self):
        with self.assertRaisesRegex(ValueError, "incomplete chunk header"):
            self.inspect_bytes(form(b"XYZ"))

    def test_not_iff(self):
        with self.assertRaisesRegex(ValueError, "not an IFF FORM"):
            self.inspect_bytes(b"not an IFF file")


if __name__ == "__main__":
    unittest.main()
