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
    passed = result.returncode == 0
    print(json.dumps({**metadata, **reference, "qualified": passed, "cli_exit": result.returncode,
                      "cli_stdout": result.stdout.strip()[:2048],
                      "cli_stderr": result.stderr.strip()[:2048]}, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
