#!/usr/bin/env python3
"""Independent minimal ILBM planar pixel-index reference decoder.

Supports 1..8 bitplanes, uncompressed and ByteRun1 BODY, optional mask plane.
Rejects HAM/EHB display modes. No AmiTerrain C code is used.
"""
import pathlib


def decode(data):
    if len(data) < 12 or data[:4] != b"FORM" or data[8:12] != b"ILBM":
        raise ValueError("not FORM ILBM")
    if int.from_bytes(data[4:8], "big") + 8 != len(data):
        raise ValueError("invalid FORM length")
    chunks = {}
    pos = 12
    while pos < len(data):
        if pos + 8 > len(data):
            raise ValueError("truncated chunk")
        tag = data[pos:pos + 4]
        size = int.from_bytes(data[pos + 4:pos + 8], "big")
        end = pos + 8 + size
        if end + (size & 1) > len(data):
            raise ValueError("chunk exceeds FORM")
        if tag in chunks and tag in (b"BMHD", b"BODY", b"CAMG"):
            raise ValueError("duplicate essential chunk")
        chunks[tag] = data[pos + 8:end]
        pos = end + (size & 1)
    if b"BMHD" not in chunks or b"BODY" not in chunks:
        raise ValueError("missing BMHD or BODY")
    bmhd = chunks[b"BMHD"]
    if len(bmhd) != 20:
        raise ValueError("invalid BMHD")
    width, height = int.from_bytes(bmhd[:2], "big"), int.from_bytes(bmhd[2:4], "big")
    planes, masking, compression = bmhd[8], bmhd[9], bmhd[10]
    if not width or not height or not 1 <= planes <= 8:
        raise ValueError("unsupported geometry or planes")
    if masking not in (0, 1) or compression not in (0, 1):
        raise ValueError("unsupported masking or compression")
    camg = chunks.get(b"CAMG", b"")
    if camg:
        if len(camg) != 4 or int.from_bytes(camg, "big") & (0x0800 | 0x0080):
            raise ValueError("unsupported CAMG HAM/EHB")
    stride = ((width + 15) // 16) * 2
    row_bytes = stride * (planes + (masking == 1))
    expected = row_bytes * height
    source = chunks[b"BODY"]
    if compression == 0:
        if len(source) != expected:
            raise ValueError("invalid BODY size")
        raw = source
    else:
        raw = bytearray()
        i = 0
        # ByteRun1 packets may not cross an ILBM plane-row boundary.
        for _ in range(height * (planes + (masking == 1))):
            row = bytearray()
            while len(row) < stride:
                if i >= len(source):
                    raise ValueError("truncated ByteRun1 plane row")
                code = source[i]
                i += 1
                if code <= 127:
                    count = code + 1
                    if i + count > len(source):
                        raise ValueError("truncated ByteRun1 literal")
                    if len(row) + count > stride:
                        raise ValueError("ByteRun1 packet crosses plane-row boundary")
                    row.extend(source[i:i + count])
                    i += count
                elif code >= 129:
                    if i >= len(source):
                        raise ValueError("truncated ByteRun1 repeat")
                    count = 257 - code
                    if len(row) + count > stride:
                        raise ValueError("ByteRun1 packet crosses plane-row boundary")
                    row.extend(source[i:i + 1] * count)
                    i += 1
            raw.extend(row)
        if any(code != 0x80 for code in source[i:]):
            raise ValueError("trailing ByteRun1 BODY bytes")
    pixels = bytearray(width * height)
    for y in range(height):
        base = y * row_bytes
        for plane in range(planes):
            offset = base + plane * stride
            for x in range(width):
                if raw[offset + x // 8] & (0x80 >> (x % 8)):
                    pixels[y * width + x] |= 1 << plane
    return width, height, bytes(pixels)


def main():
    import argparse
    import hashlib
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample", type=pathlib.Path)
    args = parser.parse_args()
    width, height, pixels = decode(args.sample.read_bytes())
    print(json.dumps({"width": width, "height": height,
                      "pixel_indices_sha256": hashlib.sha256(pixels).hexdigest(),
                      "pixel_count": len(pixels)}, sort_keys=True))


if __name__ == "__main__":
    main()
