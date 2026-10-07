# ATF — AmiTerrain Format

ATF is AmiTerrain's native, Amiga-friendly terrain container. Version **0.1** is deliberately small and lossless.

All multibyte integers are unsigned **big-endian**. Chunks use IFF semantics: a 4-byte ID, 32-bit data length, data, and one pad byte when the data length is odd. Unknown chunks must be skipped.

```text
FORM <size> ATFN
  HEAD  8
  SIZE  8
  HMAP  width * height * 2
```

## HEAD

```text
u16 major = 0
u16 minor = 1
u32 flags = 0
```

Readers must reject unsupported major/minor versions for now.

## SIZE

```text
u32 width
u32 height
```

Both dimensions must be non-zero.

## HMAP

Exactly `width * height` unsigned 16-bit samples in row-major order, stored big-endian. The canonical M0 elevation domain is 0..65535; physical units and georeferencing are intentionally not assigned yet.

## Extension rules

Unknown chunks are skippable. Future optional chunks may include:

```text
META CRS  BBOX TILE MATL BIOM WATR VEGT CAMR LITE ANIM
```

The core heightfield must remain usable without scene or GIS metadata. Compression, if introduced, will be optional rather than changing the meaning of HMAP.


### `CRS ` — 64 or 76 bytes, optional

Generic spatial metadata extension. Older readers may safely ignore this chunk. Readers supporting the original 64-byte form must also accept the 76-byte extension and ignore trailing fields they do not understand.

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | CRS type: 0 unknown, 1 geographic, 2 projected |
| 4 | u32 | coordinate units: 0 unknown, 1 degrees, 2 metres, 3 feet |
| 8 | u32 | EPSG code, 0 when unknown |
| 12 | u32 | reserved, zero |
| 16 | 6 × f64 | affine transform |
| 64 | i32 | source projection-system code, 0 if unknown (76-byte form) |
| 68 | i32 | source projection-zone code, 0 if unknown (76-byte form) |
| 72 | i32 | source horizontal-datum code, 0 if unknown (76-byte form) |

The affine transform maps raster coordinates to world coordinates:

`X = T0 + column*T1 + row*T2`

`Y = T3 + column*T4 + row*T5`

All numeric values use big-endian byte order. `GEO ` remains the compatibility representation for geographic north-up rasters; writers currently emit both chunks when geographic metadata is available.

## Canonical height samples and missing data

The M0 terrain core stores one unsigned 16-bit sample per cell. Signed integer elevations are represented with a +32768 offset when imported into this canonical form.

Sample value **0 is reserved for missing/void terrain data**. Importers must translate a source format's documented missing-data sentinel to 0 rather than treating it as a real elevation. Consequently, signed elevation -32768 is not representable as an ordinary canonical terrain height in M0.

Current mappings include:

- DTED signed-magnitude void value -32767 -> 0
- USGS DEM documented void elevation -32767 -> 0

Writers and processing stages must preserve sample 0 as missing data unless an operation explicitly fills or repairs voids. This convention is part of the M0 interchange contract and applies equally to ATF HMAP samples.
