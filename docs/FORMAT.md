# Native terrain format draft

AmiTerrain's native format is intended to remain Amiga-friendly and extensible.

Working name: **ATF — AmiTerrain Format**.

The container is based on IFF semantics.

Proposed form:

```text
FORM ATFN
  HEAD
  SIZE
  HMAP
  META
```

Future optional chunks may include:

```text
CRS 
BBOX
TILE
MATL
BIOM
WATR
VEGT
CAMR
LITE
ANIM
```

## Principles

1. Unknown chunks must be safely skippable.
2. Core height data must be usable without scene/render metadata.
3. Byte order and numeric representation must be explicitly documented.
4. The format must remain practical on classic Amiga systems.
5. Geospatial metadata may be preserved without making GIS support mandatory.
6. No compression is required for the first version; compression may be introduced as an optional chunk encoding later.
