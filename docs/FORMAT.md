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
