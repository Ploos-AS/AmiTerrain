# AmiTerrain

A modern terrain generation and landscape rendering system for classic Amiga computers.

AmiTerrain is inspired by Vista/VistaPro, Scenery Animator and World Construction Set, but is designed as a new native Amiga toolchain rather than a clone.

## Goals

- Native Amiga terrain generation and rendering
- Deterministic procedural terrain generation
- Import real-world elevation data
- Tile-based processing for worlds larger than available RAM
- Open file formats and documented interchange
- CLI, library API and later ARexx/GUI interfaces
- Useful both on real Amiga hardware and high-speed emulators/VMs
- Integration with AmiScene-SDK and the wider Ploos Amiga toolchain

## Initial architecture

- `libamiterrain` — reusable terrain engine
- `amiterrain-cli` — headless command-line frontend
- `amiterrain-worker` — future distributed renderer/worker
- GUI — later milestone
- ARexx port — later milestone

## M0

M0 establishes the deterministic terrain core and interchange baseline.

Planned M0 scope:

- deterministic seeded terrain generation
- RAW heightmap import/export
- PGM heightmap import/export
- IFF-based native terrain container draft
- terrain statistics and validation
- CLI suitable for CI
- host-side tests
- Amiga cross-build skeleton

Rendering, erosion, GIS import and GUI are intentionally deferred until the core data model is stable.

## Terrain formats roadmap

### M0
- RAW 8/16-bit heightmaps
- PGM
- native IFF terrain container

### Later
- ILBM/IFF
- PNG 16-bit grayscale
- ASCII Grid / ASC
- XYZ
- HGT / SRTM
- USGS DEM
- DTED
- GeoTIFF
- OBJ / PLY / STL
- LightWave / Imagine / POV-Ray interchange

## License

Software: MIT License.

Documentation: CC BY 4.0.
