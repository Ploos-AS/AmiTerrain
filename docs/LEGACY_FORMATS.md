# Legacy terrain compatibility

AmiTerrain treats compatibility with classic landscape generators as a first-class goal.

## Status vocabulary

- **Verified** — implemented from a documented specification/source and covered by fixtures/tests.
- **Experimental** — implemented, but more original-file fixtures are required.
- **Research** — identified and being documented; no compatibility claim yet.

No format is called compatible solely because of a filename extension.

## Vista / VistaPro

Priority: **P0**.

### Vista binary elevation array — implemented

AmiTerrain currently has a lossless signed 16-bit Motorola/big-endian array adapter with explicit dimensions and south-west/east/north orientation handling.

This adapter is deliberately named **Vista binary array**. It must not be confused with the richer native VistaPro DEM file below.

### Native VistaPro DEM — Experimental import

AmiTerrain implements native compressed VistaPro DEM import as the explicit CLI format `vistapro-dem`. The implementation follows the released WCS/VNS VistaPro importer rather than guessing from a `.dem` extension.

Current source-backed contract:

- 32-byte file identifier begins with `Vista DEM File`
- compression/header fields are Motorola/big-endian
- square sizes 258, 514, 1026 and 2050 are recognised
- elevation data begins at byte 2048
- each raster begins with a big-endian 16-bit compressed byte count
- raster payload uses VistaPro's ByteRun1-like coding
- decoded data starts with a signed 16-bit base elevation
- signed 8-bit deltas follow; `-128` escapes to a new signed 16-bit base
- the southern raster is stored first and is reversed into AmiTerrain's north/top-first canonical memory order

Synthetic regression coverage includes normal deltas, the new-base escape, orientation, and truncated-input rejection. Status remains **Experimental** until original VistaPro files can be checked without redistributing copyrighted sample data.

This codec remains separate from the raw Vista binary-array adapter.

Other Vista/VistaPro targets:

- native VistaPro ColorMap files
- USGS DEM workflows used by Vista/VistaPro
- original project/camera metadata where sufficiently documented

## World Construction Set (WCS)

Priority: **P0**.

The released 3D Nature source is the normative research reference for WCS compatibility.

The Amiga source itself exposes separate import choices for:

- Binary Array
- WCS DEM
- Z Buffer
- ASCII Array
- Vista DEM
- IFF
- DTED

and export choices for:

- Binary Array
- WCS DEM
- Z Buffer
- Color Map
- Gray IFF
- Color IFF

### WCS ELEV/DEM — Experimental

AmiTerrain implements source-backed WCS ELEV import and v1.02 export with synthetic regression coverage, including geographic metadata round trips. It remains **Experimental** pending qualification against original redistributable or privately checksum-verified WCS files.

A generic raw binary DEM is not treated as the native WCS DEM.

### Broader WCS interchange

Historically relevant inputs/outputs to support include:

- USGS DEM
- USGS DLG
- World Data Bank vectors
- AutoCAD DXF
- DTED
- IFF imagery
- Z-buffer interchange
- LightWave motion interchange
- later WCS formats where useful and feasible

Project/scene interchange is a separate compatibility layer from terrain interchange.

## Scenery Animator

Priority: **P0 research / P1 implementation**.

Collect original manuals, sample landscapes and known-good files before declaring exact native-format support. Common interchange formats can be implemented independently while native files remain under research.

## Compatibility fixtures

Never commit copyrighted commercial sample packs unless redistribution is explicitly permitted.

Tests should use:

1. freely redistributable original fixtures;
2. synthetic files generated from documented format specifications;
3. checksums/metadata for privately held originals where redistribution is not permitted;
4. malformed/truncated fixtures for parser hardening.

Where export is supported, a legacy-format round trip must preserve all representable terrain samples and metadata.
