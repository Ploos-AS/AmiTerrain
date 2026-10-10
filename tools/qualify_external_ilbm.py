#!/usr/bin/env python3
"""Qualify a locally downloaded third-party ILBM file using AmiTerrain.

Usage: python3 tools/qualify_external_ilbm.py sample.iff [--cli ./amiterrain]
Prints JSON evidence; never copies the sample into the repository.
"""
import argparse
import json
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from qualify_ilbm import inspect
from reference_ilbm import decode
import hashlib
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample", type=pathlib.Path)
    parser.add_argument("--cli", default="./amiterrain")
    args = parser.parse_args()
    try:
        metadata = inspect(args.sample)
    except (OSError, ValueError) as exc:
        print(json.dumps({"file": str(args.sample), "qualified": False, "error": str(exc)}))
        return 1
    if metadata["form"] != "ILBM":
        print(json.dumps({**metadata, "qualified": False, "error": "not FORM ILBM"}))
        return 1
    try:
        width, height, indices = decode(args.sample.read_bytes())
        planes = metadata["planes"]
        maximum = (1 << planes) - 1
        expected_be16 = b"".join(((v * 65535 // maximum).to_bytes(2, "big")) for v in indices)
        reference = {"reference_decoder": "python-planar-v1",
                     "reference_heightmap_be16_sha256": hashlib.sha256(expected_be16).hexdigest(),
                     "reference_heightmap_sample_count": len(indices),
                     "reference_width": width, "reference_height": height,
                     "reference_pixel_indices_sha256": hashlib.sha256(indices).hexdigest(),
                     "reference_pixel_count": len(indices)}
    except (OSError, ValueError) as exc:
        print(json.dumps({**metadata, "qualified": False,
                          "error": f"reference decoder: {exc}"}, sort_keys=True))
        return 1
    try:
        result = subprocess.run([args.cli, "validate", str(args.sample)],
                                capture_output=True, text=True, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(json.dumps({**metadata, **reference, "qualified": False, "error": str(exc)}))
        return 1
    comparison = {"heightmap_compared": False, "heightmap_matches_reference": False,
                  "heightmap_status": "validation_failed" if result.returncode else "not_run"}
    if result.returncode == 0:
        try:
            with tempfile.TemporaryDirectory(prefix="amiterrain-ilbm-") as temp:
                raw_path = pathlib.Path(temp) / "decoded.raw"
                converted = subprocess.run(
                    [args.cli, "convert", str(args.sample), str(raw_path)],
                    capture_output=True, text=True, timeout=30, check=False)
                if converted.returncode == 0:
                    actual = raw_path.read_bytes()
                    comparison = {"heightmap_compared": True,
                                  "heightmap_matches_reference": actual == expected_be16,
                                  "heightmap_status": "matched" if actual == expected_be16 else "mismatch",
                                  "heightmap_actual_be16_sha256": hashlib.sha256(actual).hexdigest(),
                                  "heightmap_actual_byte_count": len(actual)}
                else:
                    comparison["heightmap_status"] = "conversion_failed"
                    comparison["heightmap_error"] = converted.stderr.strip()[:2048]
        except (OSError, subprocess.TimeoutExpired) as exc:
            comparison["heightmap_status"] = "output_unavailable" if isinstance(exc, FileNotFoundError) else "conversion_exception"
            comparison["heightmap_error"] = str(exc)
    passed = result.returncode == 0 and comparison["heightmap_matches_reference"]
    print(json.dumps({**metadata, **reference, **comparison,
                      "qualified": passed, "cli_exit": result.returncode,
                      "cli_stdout": result.stdout.strip()[:2048],
                      "cli_stderr": result.stderr.strip()[:2048]}, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
