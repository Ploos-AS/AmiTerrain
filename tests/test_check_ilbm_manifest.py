import json
import pathlib
import tempfile
import unittest
import importlib.util

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_ilbm_manifest", ROOT / "tools" / "check_ilbm_manifest.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ManifestTests(unittest.TestCase):
    def test_empty_manifest(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / "manifest.json"
            p.write_text(json.dumps({"samples": []}))
            self.assertEqual(module.validate(json.loads(p.read_text()), p.parent), 0)

    def test_valid_report(self):
        with tempfile.TemporaryDirectory() as d:
            base = pathlib.Path(d)
            digest = "a" * 64
            (base / "report.json").write_text(json.dumps({"sha256": digest, "form": "ILBM", "qualified": True, "cli_exit": 0}))
            manifest = {"samples": [{"id": "sample-1", "sha256": digest,
                                     "source_url": "https://example.invalid/sample.iff",
                                     "report": "report.json"}]}
            self.assertEqual(module.validate(manifest, base), 1)

    def test_digest_mismatch(self):
        with tempfile.TemporaryDirectory() as d:
            base = pathlib.Path(d)
            (base / "report.json").write_text(json.dumps({"sha256": "b" * 64, "form": "ILBM", "qualified": True, "cli_exit": 0}))
            manifest = {"samples": [{"id": "sample-1", "sha256": "a" * 64,
                                     "source_url": "https://example.invalid/sample.iff",
                                     "report": "report.json"}]}
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                module.validate(manifest, base)


    def test_false_independent_claim(self):
        with tempfile.TemporaryDirectory() as d:
            base = pathlib.Path(d)
            digest = "c" * 64
            (base / "report.json").write_text(json.dumps({
                "sha256": digest, "form": "ILBM", "qualified": True, "cli_exit": 0
            }))
            sample = {"id": "sample-1", "sha256": digest,
                      "source_url": "https://example.invalid/sample.iff",
                      "report": "report.json", "independent_comparison": False,
                      "comparison_result": "pass"}
            with self.assertRaisesRegex(ValueError, "cannot be claimed"):
                module.validate({"samples": [sample]}, base)

    def test_independent_comparison_requires_method(self):
        with tempfile.TemporaryDirectory() as d:
            base = pathlib.Path(d)
            digest = "d" * 64
            (base / "report.json").write_text(json.dumps({
                "sha256": digest, "form": "ILBM", "qualified": True, "cli_exit": 0
            }))
            sample = {"id": "sample-1", "sha256": digest,
                      "source_url": "https://example.invalid/sample.iff",
                      "report": "report.json", "independent_comparison": True,
                      "comparison_result": "pass"}
            with self.assertRaisesRegex(ValueError, "independent comparison evidence"):
                module.validate({"samples": [sample]}, base)


if __name__ == "__main__":
    unittest.main()
