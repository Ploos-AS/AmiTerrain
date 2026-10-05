# Legacy terrain compatibility

AmiTerrain treats compatibility with classic landscape generators as a first-class goal.

## Policy

Legacy support has three levels:

- **Verified** — format is documented or backed by source code and regression fixtures.
- **Experimental** — structure is sufficiently understood for guarded import/export, but needs more fixtures.
- **Research** — known product/file family, but not enough evidence yet to claim compatibility.

No format will be labelled compatible solely from a filename extension.

## Vista / VistaPro

Priority: **P0**.

Documented VistaPro binary DEM interchange is simple signed 16-bit Motorola/big-endian sample data. Samples begin at the south-west corner, proceed east across a row, then north row-by-row.

Planned adapters:

- VistaPro binary DEM import/export
- VistaPro DEM landscape files
- VistaPro ColorMap files
- USGS DEM workflows used by Vista/VistaPro

The raw binary adapter will expose dimensions explicitly because the byte stream does not self-describe them.

## World Construction Set (WCS)

Priority: **P0**.

WCS is unusually attractive because Amiga-era source code is available, allowing format support to be implemented from code rather than guesswork.

Targets include:

- WCS binary DEM
- USGS DEM
- USGS DLG
- World Data Bank vectors
- AutoCAD DXF
- WCS project/scene interchange where practical
- LightWave motion interchange
- IFF-ZBUF / raw Z-buffer interchange

WCS compatibility should preserve useful metadata where ATF has an equivalent chunk.

## Scenery Animator

Priority: **P0 research / P1 implementation**.

We will collect original manuals, sample landscapes and known-good files before declaring exact native-format support. Until then, AmiTerrain should support the common elevation interchange formats used around it and maintain a fixture-driven reverse-engineering track.

## Compatibility fixtures

Never commit copyrighted commercial sample packs unless redistribution is explicitly permitted. Tests should use:

1. freely redistributable original fixtures;
2. synthetic files generated from documented format specifications;
3. checksums/metadata for privately held originals where redistribution is not permitted.

Each importer gets malformed/truncated-file tests as well as successful round trips where export is supported.
