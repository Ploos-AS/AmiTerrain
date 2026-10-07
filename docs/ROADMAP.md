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

## M2.5 — Classic landscape compatibility

Preserve and interoperate with historically important landscape generators instead of treating them as image-only sources.

- Vista / VistaPro native terrain and scene formats
- Scenery Animator formats and workflows
- World Construction Set formats where practical
- other documented Amiga landscape formats as they are identified
- preservation metadata in the Terrain IR
- round-trip/native export where the original format can be reproduced safely
- authentic profiles for palette, resolution, camera, lighting and terrain semantics
- conversion of modern terrain back to original-platform-compatible data where representable

The compatibility goal has two paths: preserve native source data for authentic/original workflows, and normalize a lossless-as-practical representation for modern processing.

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

## M3.5 — Modern terrain generators

Modern open-source terrain workflows are first-class interoperability targets.

- TerraForge3D interchange
- Blender terrain workflows and open-source erosion extensions
- Procedural Terrains-style heightmap/mesh workflows
- Hydra-style hydraulic erosion workflows
- generic 16-bit heightmap exchange
- DEM/GIS to procedural terrain pipelines
- deterministic import/export fixtures for supported interchange paths

Prefer documented open formats and stable interchange (heightmaps, meshes and metadata) over coupling AmiTerrain to another application's internal implementation.

## M5 — Interchange

- OBJ
- PLY
- STL
- POV-Ray
- LightWave
- Imagine
- AmiScene-SDK integration
- GLTF/GLB
- Blender interchange
- AmiRender Terrain IR handoff
- preservation metadata sidecars where target formats cannot carry source semantics

## M6 — Native application

- Amiga GUI
- interactive preview
- project management
- ARexx port
- presets

## M7 — AmiRender integration

AmiTerrain owns terrain generation, terrain conversion, native landscape formats and Terrain IR semantics. AmiRender owns distributed rendering and modern render backends.

- hand off Terrain IR/scenes to AmiRender
- `original` path: invoke qualified original software/runtime where practical
- `authentic` path: reproduce original landscape renderer constraints
- `enhanced` path: modern POV-Ray/Blender rendering
- preserve native terrain assets alongside normalized IR
- never require destructive conversion to use the modern render farm

## M8 — Distributed/high-performance rendering

- worker protocol
- tiled render jobs
- animation frame distribution
- AmiVM/FS-UAE/WinUAE worker use
- reproducible benchmark suite
