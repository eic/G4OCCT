<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
<!-- Copyright (C) 2026 G4OCCT Contributors -->

# G4OCCT

[![CI](https://github.com/eic/G4OCCT/actions/workflows/ci.yml/badge.svg)](https://github.com/eic/G4OCCT/actions/workflows/ci.yml)
[![Documentation](https://github.com/eic/G4OCCT/actions/workflows/ci.yml/badge.svg)](https://eic.github.io/G4OCCT/)
[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](LICENSE)

Geant4 interface to Open CASCADE Technology (OCCT) geometry definitions.

G4OCCT provides a compatibility layer between
[Geant4](https://github.com/geant4/geant4) geometry descriptions and the
[Open CASCADE Technology (OCCT)](https://github.com/Open-Cascade-SAS/OCCT)
geometry framework.  The goal is to enable physics simulations to be driven
by CAD geometry imported from STEP (and other CAD exchange) files, while
retaining full compatibility with Geant4's navigation, scoring, and
visualisation subsystems.

---

## Motivation

Geant4 is the de-facto standard toolkit for high-energy physics detector
simulations.  Its geometry is traditionally hand-coded using constructive
solid geometry (CSG) primitives or tessellated meshes.  Engineering designs,
however, are almost universally stored in CAD tools and exchanged in formats
such as STEP.  G4OCCT bridges this gap by:

- Providing **accurate detector simulations** from the engineering design
  directly, eliminating geometry discrepancies between the CAD model and the
  simulation.
- **Reducing maintenance burden** — geometry changes in the CAD tool propagate
  automatically to the simulation.
- Enabling **richer geometry** — OCCT BRep (boundary representation) shapes
  can capture design intent (fillets, chamfers, swept surfaces) that CSG
  primitives cannot.

For more detail see the [Project Goals](https://eic.github.io/G4OCCT/#/goals)
documentation page.

---

## Architecture

G4OCCT uses a thin-wrapper approach: OCCT shapes are embedded inside Geant4
constructs so that the Geant4 navigator, scoring, and visualisation
infrastructure remain unchanged.

| G4OCCT class | Inherits from | Embeds |
|---|---|---|
| `G4OCCTSolid` | `G4VSolid` | `TopoDS_Shape` |
| `TGeoOCCTSolid`* | `TGeoShape` | `TopoDS_Shape` |
| `G4OCCTLogicalVolume` | `G4LogicalVolume` | `TopoDS_Shape` (optional) |
| `G4OCCTPlacement` | `G4PVPlacement` | `TopLoc_Location` |

\* Built when `BUILD_ROOT_TGEO_SUPPORT=ON`.

Navigation queries (`Inside`, `DistanceToIn/Out`, `SurfaceNormal`, …) are
delegated to OCCT BRep algorithms.  See the
[Geometry Mapping](https://eic.github.io/G4OCCT/#/geometry_mapping) and
[Solid Navigation Design](https://eic.github.io/G4OCCT/#/solid_navigation)
documentation pages for details.

---

## Requirements

| Dependency | Minimum version |
|---|---|
| CMake | 3.16 |
| C++ | 20 |
| [Geant4](https://github.com/geant4/geant4) | 11.3 |
| [OpenCASCADE (OCCT)](https://github.com/Open-Cascade-SAS/OCCT) | 7.8 |

---

## Building

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON \
  -DBUILD_ROOT_TGEO_SUPPORT=ON \
  -DCMAKE_INSTALL_PREFIX=/path/to/install

cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)
cmake --install build
```

Both `BUILD_TESTING` and `BUILD_BENCHMARKS` default to `OFF`; enable them
during development to run the CTest suite and the geantino navigator
benchmarks.

---

## Downstream Usage

After installation, integrate G4OCCT into a CMake project with:

```cmake
find_package(G4OCCT REQUIRED)
target_link_libraries(myApp PRIVATE G4OCCT::G4OCCT)
```

The installed `G4OCCTConfig.cmake` propagates the Geant4 and OCCT
dependencies automatically.

When `BUILD_ROOT_TGEO_SUPPORT=ON`, the installed package also propagates the
ROOT Geom dependency needed for `TGeoOCCTSolid`.

---

## Repository Layout

```text
G4OCCT/
├── CMakeLists.txt          # Top-level build; installs G4OCCTConfig.cmake
├── cmake/
│   └── G4OCCTConfig.cmake.in
├── include/G4OCCT/
│   ├── G4OCCTSolid.hh                       # G4VSolid wrapping TopoDS_Shape
│   ├── G4OCCTLogicalVolume.hh
│   ├── G4OCCTPlacement.hh                   # G4PVPlacement + TopLoc_Location
│   ├── G4OCCTAssemblyVolume.hh              # multi-shape STEP assembly (XDE import)
│   ├── G4OCCTAssemblyRegistry.hh            # singleton registry for plugin workflows
│   ├── G4OCCTMaterialMap.hh                 # STEP material name → G4Material* map
│   ├── G4OCCTMaterialMapReader.hh           # XML material map file reader
│   ├── G4OCCTSensitiveDetectorMap.hh        # volume name → G4VSensitiveDetector* map
│   ├── G4OCCTSensitiveDetectorMapReader.hh  # XML sensitive detector map reader
│   └── TGeoOCCTSolid.hh                     # ROOT/TGeo adapter (BUILD_ROOT_TGEO_SUPPORT)
├── src/
│   ├── G4OCCTSolid.cc
│   ├── G4OCCTSolidKernel.cc / .hh           # shared multi-tier navigation kernel
│   ├── G4OCCTLogicalVolume.cc
│   ├── G4OCCTPlacement.cc
│   ├── G4OCCTAssemblyVolume.cc
│   ├── G4OCCTAssemblyRegistry.cc
│   ├── G4OCCTMaterialMap.cc
│   ├── G4OCCTMaterialMapReader.cc
│   ├── G4OCCTSensitiveDetectorMap.cc
│   ├── G4OCCTSensitiveDetectorMapReader.cc
│   ├── TGeoOCCTSolid.cc / TGeoOCCTSolidBridge.cc
│   ├── app/g4occt/                          # standalone g4occt interactive application
│   ├── examples/                            # usage examples
│   │   ├── B1/                              # water phantom example
│   │   ├── B4c/                             # sampling calorimeter example
│   │   └── DD4hepSimpleDetector/            # DD4hep STEP-backed VXD layer
│   ├── dd4hep/                              # optional DD4hep detector element plugins
│   │   ├── G4OCCT_STEPSolid.cc
│   │   ├── G4OCCT_STEPAssembly.cc
│   │   └── G4OCCT_STEPAssemblySD.cc
│   ├── tests/                               # CTest-integrated unit tests
│   └── benchmarks/                          # geantino navigator benchmarks
└── docs/
    ├── goals.md            # Project goals, design philosophy, and roadmap
    ├── geometry_mapping.md # Geant4 ↔ OCCT class correspondence
    ├── solid_navigation.md # G4VSolid ↔ OCCT algorithm mapping
    ├── material_bridging.md
    ├── step_assembly_import.md
    ├── dd4hep_plugin.md
    └── performance.md
```

---

## Documentation

Full documentation is available at **<https://eic.github.io/G4OCCT/>**,
including:

- [**Project Overview Slides**](https://eic.github.io/G4OCCT/slides.html) —
  20-slide deck covering motivation, architecture, performance, and roadmap.
- [Project Goals](https://eic.github.io/G4OCCT/#/goals) — Vision,
  motivation, design philosophy, and roadmap.
- [Geometry Mapping](https://eic.github.io/G4OCCT/#/geometry_mapping) —
  Correspondence between Geant4 and OCCT class hierarchies.
- [Solid Navigation Design](https://eic.github.io/G4OCCT/#/solid_navigation) —
  Per-function mapping of `G4VSolid` queries to OCCT algorithms.
- [Performance Considerations](https://eic.github.io/G4OCCT/#/performance) —
  Benchmark results and optimisation strategies.
- [Material Bridging](https://eic.github.io/G4OCCT/#/material_bridging) —
  Strategies for mapping STEP/OCCT material names to `G4Material`.
- [Multi-Shape STEP Assembly Import](https://eic.github.io/G4OCCT/#/step_assembly_import) —
  Importing STEP assemblies via OCCT XDE into a Geant4 volume hierarchy.
- [DD4hep Plugin Design](https://eic.github.io/G4OCCT/#/dd4hep_plugin) —
  Using `G4OCCT_STEPSolid` and `G4OCCT_STEPAssembly` in DD4hep compact XML.
- [Geometry Test Status](https://eic.github.io/G4OCCT/#/geometry_test_status) —
  Validation coverage for all supported Geant4 primitive families.
- **Examples:**
  - [B1 — Water Phantom](https://eic.github.io/G4OCCT/#/example_b1)
  - [B4c — Sampling Calorimeter](https://eic.github.io/G4OCCT/#/example_b4c)
  - [DD4hep SimpleDetector — STEP-Backed VXD Layer](https://eic.github.io/G4OCCT/#/example_dd4hep_simpledetector)
- [API Reference](https://eic.github.io/G4OCCT/api/) — Doxygen-generated
  API documentation.

---

## Contributing

Contributor conventions (coding style, CI setup, documentation requirements,
and design principles) are described in [AGENTS.md](AGENTS.md).

---

## License

This project is licensed under the
[GNU Lesser General Public License v2.1 or later](LICENSE)
(SPDX identifier: `LGPL-2.1-or-later`).
