#!/usr/bin/env python3
"""Inspect independently sourced IFF/ILBM files without distributing them.

Usage: python3 tools/qualify_ilbm.py path/to/file.iff [...]
Outputs one JSON record per input; never copies or embeds input data.
"""
import hashlib
import json
import pathlib
import sys


def inspect(path):
    p = pathlib.Path(path)
    data = p.read_bytes()
    record = {"file": str(p), "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if len(data) < 12 or data[:4] != b"FORM":
        raise ValueError("not an IFF FORM")
    declared = int.from_bytes(data[4:8], "big")
    record["form"] = data[8:12].decode("ascii", "replace")
    record["form_size"] = declared
    if declared + 8 != len(data):
        raise ValueError("FORM length does not match file length")
    offset = 12
    chunks = []
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError("incomplete chunk header")
        kind = data[offset:offset + 4]
        size = int.from_bytes(data[offset + 4:offset + 8], "big")
        start = offset + 8
        end = start + size
        if end + (size & 1) > len(data):
            raise ValueError("chunk extends past FORM")
        label = kind.decode("ascii", "replace")
        chunks.append({"type": label, "size": size})
        if kind == b"BMHD" and size >= 20:
            h = data[start:start + 20]
            record.update(width=int.from_bytes(h[:2], "big"),
                          height=int.from_bytes(h[2:4], "big"),
                          planes=h[8], masking=h[9], compression=h[10])
        if kind == b"CAMG" and size >= 4:
            record["camg"] = int.from_bytes(data[start:start + 4], "big")
        offset = end + (size & 1)
    record["chunks"] = chunks
    return record


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    errors = 0
    for path in sys.argv[1:]:
        try:
            print(json.dumps({"ok": True, **inspect(path)}, sort_keys=True))
        except (OSError, ValueError) as exc:
            print(json.dumps({"ok": False, "file": path, "error": str(exc)}))
            errors += 1
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
