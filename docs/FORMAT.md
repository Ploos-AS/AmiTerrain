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


### `CRS ` — 64 bytes, optional

Generic spatial metadata extension. Older readers may safely ignore this chunk.

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | CRS type: 0 unknown, 1 geographic, 2 projected |
| 4 | u32 | coordinate units: 0 unknown, 1 degrees, 2 metres, 3 feet |
| 8 | u32 | EPSG code, 0 when unknown |
| 12 | u32 | reserved, zero |
| 16 | 6 × f64 | affine transform |

The affine transform maps raster coordinates to world coordinates:

`X = T0 + column*T1 + row*T2`

`Y = T3 + column*T4 + row*T5`

All numeric values use big-endian byte order. `GEO ` remains the compatibility representation for geographic north-up rasters; writers currently emit both chunks when geographic metadata is available.
