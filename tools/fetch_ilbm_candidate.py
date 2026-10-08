#!/usr/bin/env python3
"""Download a pinned external ILBM to a user-selected path, never into the repo.

Usage: python3 tools/fetch_ilbm_candidate.py /tmp/amiga_lagoon.iff
"""
import argparse
import hashlib
import json
import pathlib
import urllib.request

URL = "https://raw.githubusercontent.com/mdoege/IFFshow/master/demo_images/amiga_lagoon.iff"
EXPECTED_GIT_BLOB = "713d45a6eee3d4d8e2ee9ed17cf8b120285d2363"
EXPECTED_SIZE = 484242


def verify_blob(data, expected_size=EXPECTED_SIZE, expected_hash=EXPECTED_GIT_BLOB):
    """Return the canonical Git blob hash or reject mismatched bytes."""
    digest = hashlib.sha1(b"blob " + str(len(data)).encode() + bytes([0]) + data).hexdigest()
    if len(data) != expected_size or digest != expected_hash:
        raise ValueError("download does not match pinned Git blob")
    return digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("destination already exists; refusing overwrite")
    try:
        with urllib.request.urlopen(URL, timeout=30) as response:
            data = response.read(EXPECTED_SIZE + 1)
    except OSError as exc:
        parser.error(f"download failed: {exc}")
    try:
        blob_hash = verify_blob(data)
    except ValueError as exc:
        parser.error(f"{exc}; refusing to write")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(json.dumps({"path": str(args.output), "size": len(data),
                      "git_blob_sha1": blob_hash, "sha256": hashlib.sha256(data).hexdigest()}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
