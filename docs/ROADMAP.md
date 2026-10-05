# AmiTerrain roadmap

## M0 — Terrain core

Purpose: establish a small, deterministic, testable terrain engine before adding rendering or GUI complexity.

### Deliverables

- Terrain grid data model
- 16-bit canonical elevation representation
- deterministic PRNG and seed handling
- basic procedural generator
- RAW import/export
- PGM import/export
- native IFF container draft
- CLI:
  - generate
  - info
  - convert
  - validate
- host-side unit/regression tests
- Amiga cross-build skeleton

### Exit criteria

- Same seed and parameters produce identical terrain data
- Round-trip RAW and PGM tests pass
- Native container can be written and read back losslessly
- CLI is scriptable and returns useful exit codes
- No GUI dependency in the core library

## M1 — Terrain operations

- resampling
- crop
- normalize
- combine
- masks
- fractal/fBm generators
- ridged multifractal
- domain warping
- thermal erosion
- hydraulic erosion baseline

## M2 — Preview renderer

- heightfield renderer
- camera and sun
- terrain shading
- palette/true-colour output paths
- low-memory tiled rendering
- IFF/ILBM output

## M3 — Real-world terrain

- ASC
- XYZ
- HGT/SRTM
- USGS DEM
- DTED
- GeoTIFF subset/import path
- coordinate metadata and bounding boxes

## M4 — Scene/ecosystem system

- water
- atmosphere/fog
- snow line
- biome masks
- vegetation/ecosystems
- rivers/watersheds

## M5 — Interchange

- OBJ
- PLY
- STL
- POV-Ray
- LightWave
- Imagine
- AmiScene-SDK integration

## M6 — Native application

- Amiga GUI
- interactive preview
- project management
- ARexx port
- presets

## M7 — Distributed/high-performance rendering

- worker protocol
- tiled render jobs
- animation frame distribution
- AmiVM/FS-UAE/WinUAE worker use
- reproducible benchmark suite
