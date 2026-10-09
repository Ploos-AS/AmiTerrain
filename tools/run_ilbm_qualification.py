#!/usr/bin/env python3
"""Run the pinned external ILBM qualification workflow in one command.

Example: python3 tools/run_ilbm_qualification.py /tmp/amiterrain-qualification
Does not vendor third-party artwork; qualification compares independent Python and native C decoded heights.
"""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_dir", type=pathlib.Path)
    parser.add_argument("--cli", default=str(ROOT / "amiterrain"))
    args = parser.parse_args()
    target = args.output_dir.resolve()
    sample = target / "amiga_lagoon.iff"
    report = target / "amiga_lagoon.qualification.json"
    if sample.exists() or report.exists():
        parser.error("output already exists; refusing to overwrite qualification evidence")
    target.mkdir(parents=True, exist_ok=True)
    download = subprocess.run([sys.executable, str(ROOT / "tools" / "fetch_ilbm_candidate.py"),
                               str(sample)], check=False)
    if download.returncode:
        report.write_text(json.dumps({"qualified": False, "stage": "download",
                                      "error": "pinned external sample download or integrity verification failed"}) + chr(10),
                          encoding="utf-8")
        print(f"qualification report: {report}", file=sys.stderr)
        return download.returncode
    with report.open("x", encoding="utf-8") as out:
        result = subprocess.run([sys.executable, str(ROOT / "tools" / "qualify_external_ilbm.py"),
                                 str(sample), "--cli", args.cli],
                                stdout=out, stderr=subprocess.PIPE, text=True, check=False)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    print(f"qualification report: {report}")
    print("NOTE: Python-to-C height comparison is performed; third-party provenance and rights still require review.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
