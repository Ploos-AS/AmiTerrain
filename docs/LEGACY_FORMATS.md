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


## DTED — Experimental

Priority: **P0**.

AmiTerrain implements DTED terrain import through the explicit CLI format `dted`. The current reader follows the standard DTED record structure rather than relying only on historical application-specific loaders.

Covered by synthetic regression tests:

- UHL / DSI / ACC record layout
- dimensions from UHL
- west-to-east data profiles
- south-to-north elevations converted to AmiTerrain north/top-first order
- 16-bit big-endian signed-magnitude elevations
- `-32767` DTED void/nodata handling
- per-profile 32-bit checksum validation
- UHL latitude/longitude origin and sample intervals
- preservation of geographic metadata in AmiTerrain's canonical representation

The same codec is intended for DTED Level 0, 1 and 2 (`.dt0`, `.dt1`, `.dt2`) because the level changes resolution rather than the fundamental profile encoding.

Status remains **Experimental** until qualification against independent real-world DTED files.

## SRTM / HGT — Experimental

Priority: **P1**.

AmiTerrain implements SRTM-style HGT import through the CLI format `hgt`, with automatic detection for `.hgt` and `.HGT`.

Current regression coverage includes:

- square raster dimensions inferred from file size
- signed 16-bit big-endian elevation samples
- source void value `-32768` mapped to AmiTerrain canonical missing sample `0`
- north/top-first raster ordering
- standard tile-name georeferencing such as `N58E007.hgt`
- one-degree tile affine transform with spacing `1/(N-1)`
- HGT -> ATF CLI conversion and checksum preservation

Status remains **Experimental** until the reader is qualified against independent real SRTM tiles and malformed/truncated real-world cases.

## ESRI ASCII Grid — Experimental

Priority: **P1**.

AmiTerrain implements ESRI ASCII Grid import through the CLI format `asc`, with automatic detection for `.asc` and `.ASC`.

Current regression and CI coverage includes:

- `ncols` and `nrows`
- `xllcorner` / `yllcorner`
- `xllcenter` / `yllcenter`
- `cellsize`
- optional `NODATA_value` mapped to AmiTerrain canonical missing sample `0`
- north/top-first raster ordering
- half-cell adjustment for corner-referenced grids
- affine transform preservation through ASC -> ATF conversion
- positive, zero and negative integer elevations

The grid header provides spatial placement but does not by itself reliably identify a coordinate reference system. AmiTerrain therefore preserves the affine transform while leaving CRS type/units/EPSG unknown unless projection information is supplied by a future sidecar-aware import path.

Floating-point elevations are currently rounded to the M0 integer canonical height representation.

Status remains **Experimental** pending qualification against independent real-world grids, additional header-order/case variants, and projection sidecar handling.

## IFF/ILBM heightmaps — Experimental

Priority: **P0 Amiga interchange**.

AmiTerrain reads and writes standard Amiga `FORM ILBM` heightmaps. ILBM is intentionally treated as classic Amiga interchange rather than the lossless native terrain master format; AmiTerrain's `FORM ATFN` container retains the full canonical 16-bit terrain representation and terrain metadata.

Current ILBM reader support:

- standard `BMHD` and `BODY` chunks
- 1–8 planar bitplanes
- Amiga word-aligned bitplane rows
- uncompressed BODY data
- ByteRun1 compression
- optional mask plane
- unknown IFF chunks skipped using normal chunk/padding rules
- planar pixel values scaled deterministically to canonical 16-bit samples

Current writer contract:

- 8 bitplanes / 256 height levels
- uncompressed planar BODY
- standard 256-entry grayscale `CMAP`
- correct word-aligned rows
- canonical 16-bit heights deterministically quantized to 8-bit ILBM values

Regression coverage includes raw and ByteRun1 fixtures, 8-plane reconstruction, non-word-sized image widths, exact 8-bit round-trip, CLI conversion, and binary validation of the grayscale palette.

The generic `.iff` suffix is not blindly auto-detected as ILBM because IFF is a container used by many Amiga data types. `.ilbm` is the unambiguous CLI autodetect suffix.

Status remains **Experimental** until qualified against a representative set of independently produced Amiga ILBM files and applications.

## Regular XYZ grid — Experimental

Priority: **P1**.

AmiTerrain implements text XYZ import through the CLI format `xyz`, with automatic detection for `.xyz` and `.XYZ`.

The current contract is deliberately a **regular raster grid represented as X Y Z points**, not an arbitrary point cloud. Rows must have consistent X positions and Y spacing; irregular input is rejected rather than silently interpolated.

Current regression coverage includes:

- automatic row/column discovery
- positive or negative regular Y spacing
- signed elevations mapped to the M0 canonical height representation
- X/Y affine transform reconstruction
- explicit rejection of irregular point placement
- XYZ -> ATF CLI conversion

XYZ does not inherently identify its CRS, so CRS type, units and EPSG remain unknown while the numeric affine transform is preserved.

General point-cloud gridding, TIN construction and interpolation belong to a future terrain-processing layer rather than this deterministic interchange reader.

Status remains **Experimental** pending qualification against independent XYZ raster exports and broader numeric-format variants.

## Scenery Animator

Priority: **P0 research / P1 implementation**.

Period reviews describe Scenery Animator 4.x landscapes as DEM terrain data, but current research has not established a separate, source-backed native Scenery Animator terrain container. AmiTerrain therefore does **not** invent or claim a `scenery-dem` codec.

Research/qualification plan:

1. inspect original manuals and legally usable landscape files for versions 1.x through 4.x;
2. record file signatures, dimensions, byte order and elevation semantics before implementing a native adapter;
3. compare known landscape files against Vista/VistaPro DEM and generic DEM representations;
4. preserve checksums/metadata for original commercial fixtures that cannot be redistributed;
5. only promote native compatibility after independent files from more than one Scenery Animator version pass.

Interchange compatibility can proceed independently. Priority formats useful to classic and modern Scenery workflows are DTED, USGS DEM and IFF height/elevation data; modern extensions include SRTM/HGT, ESRI ASCII Grid, Terragen and GeoTIFF.

Scenery scene/object/project compatibility is a separate layer from terrain compatibility and should not be conflated with DEM import.

## Compatibility fixtures

Never commit copyrighted commercial sample packs unless redistribution is explicitly permitted.

Tests should use:

1. freely redistributable original fixtures;
2. synthetic files generated from documented format specifications;
3. checksums/metadata for privately held originals where redistribution is not permitted;
4. malformed/truncated fixtures for parser hardening.

Where export is supported, a legacy-format round trip must preserve all representable terrain samples and metadata.
