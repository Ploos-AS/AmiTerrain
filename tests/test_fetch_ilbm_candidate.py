import hashlib
import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("fetch_ilbm_candidate", ROOT / "tools" / "fetch_ilbm_candidate.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def git_hash(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + bytes([0]) + data).hexdigest()


class BlobVerificationTests(unittest.TestCase):
    def test_exact_blob(self):
        data = b"FORM" + bytes(range(32))
        self.assertEqual(module.verify_blob(data, len(data), git_hash(data)), git_hash(data))

    def test_wrong_length(self):
        data = b"FORM"
        with self.assertRaisesRegex(ValueError, "pinned Git blob"):
            module.verify_blob(data, len(data) + 1, git_hash(data))

    def test_wrong_hash(self):
        data = b"FORM"
        with self.assertRaisesRegex(ValueError, "pinned Git blob"):
            module.verify_blob(data, len(data), "0" * 40)

    def test_changed_payload_same_length(self):
        data = b"FORM"
        with self.assertRaises(ValueError):
            module.verify_blob(b"FORX", len(data), git_hash(data))


if __name__ == "__main__":
    unittest.main()
