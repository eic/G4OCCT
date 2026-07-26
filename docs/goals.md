<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
<!-- Copyright (C) 2026 G4OCCT Contributors -->

# G4OCCT — Project Goals and Design Philosophy

## Vision

G4OCCT provides a compatibility layer between
[Geant4](https://github.com/geant4/geant4) geometry descriptions and the
[Open CASCADE Technology (OCCT)](https://github.com/Open-Cascade-SAS/OCCT)
geometry framework.  The ultimate goal is to enable physics simulations to be
driven by CAD geometry imported from STEP (and other CAD exchange) files, while
retaining full compatibility with Geant4's navigation, scoring, and
visualisation subsystems.

---

## Motivation

Geant4 is the de-facto standard toolkit for high-energy physics detector
simulations.  Its geometry is traditionally hand-coded using constructive
solid geometry (CSG) primitives (boxes, spheres, tubes, …) or occasionally
tessellated meshes.  Engineering designs, however, are almost universally
stored in CAD tools and exchanged in formats such as STEP.  Bridging the gap
between CAD geometry and Geant4 geometry enables:

* **Accurate detector simulations** from the engineering design directly,
  eliminating geometry discrepancies between the CAD model and the simulation.
* **Reduced maintenance burden** — geometry changes in the CAD tool propagate
  automatically to the simulation.
* **Richer geometry** — OCCT BRep (boundary representation) shapes can capture
  design intent (fillets, chamfers, swept surfaces) that CSG primitives cannot.

---

## Scope

| In scope | Out of scope (v0.x) |
|---|---|
| OCCT BRep → Geant4 solid wrapper | Full OCCT-native navigator |
| STEP file import via OCCT | GDML export |
| Multi-shape STEP assembly import | GPU navigation |
| CTest-based validation suite | Magnetic field integration |
| Navigator performance benchmarks | |
| Material bridging (`G4OCCTMaterialMap`, `G4OCCTMaterialMapReader`) | |
| Sensitive detector mapping (`G4OCCTSensitiveDetectorMap/Reader`) | |
| Assembly volume registry (`G4OCCTAssemblyRegistry`) | |
| ROOT/TGeo adapter (`TGeoOCCTSolid`, optional via `BUILD_ROOT_TGEO_SUPPORT`) | |
| DD4hep detector element plugins (`G4OCCT_STEPSolid`, `G4OCCT_STEPAssembly`) | |
| Standalone `g4occt` interactive application | |
| Design documents | |

---

## Design Philosophy

### 1. Thin wrapper, not a replacement

G4OCCT wraps OCCT shapes *within* Geant4 constructs (`G4VSolid`,
`G4LogicalVolume`, `G4VPhysicalVolume`).  The Geant4 navigator, scoring, and
visualisation infrastructure remain unchanged; only the solid-geometry
queries (`Inside`, `DistanceToIn`, `DistanceToOut`) are delegated to OCCT
algorithms.

### 2. Standard CMake integration

Downstream projects integrate G4OCCT via:

```cmake
find_package(G4OCCT REQUIRED)
target_link_libraries(myApp PRIVATE G4OCCT::G4OCCT)
```

The installed `G4OCCTConfig.cmake` propagates the Geant4 and OCCT
dependencies automatically, so users do not need to manually find either.

### 3. Incremental implementation

The initial codebase ships stub implementations of all required virtual
functions.  This allows the CMake build, CI, and test infrastructure to be
validated before committing to a particular navigation algorithm.  Each
stub is annotated with a `TODO` comment and a pointer to the relevant OCCT
API.

### 4. Validated against standard shapes

The test suite in `src/tests/` exercises all Geant4 primitive shapes
(G4Box, G4Sphere, G4Tubs, G4Cons, G4Trd, G4Trap, …) against their OCCT BRep
equivalents, ensuring that navigation results agree to within numerical
precision before the library is released.

### 5. Benchmark-driven optimisation

The benchmark suite in `src/benchmarks/` provides a reproducible comparison
of navigator throughput (steps per second for geantinos) between native
Geant4 solids and G4OCCTSolid wrappers.  Performance regressions are
detectable early in the development cycle.

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
│   ├── TGeoOCCTSolid.cc / TGeoOCCTSolidBridge.cc  # ROOT/TGeo adapter
│   ├── app/g4occt/                          # standalone g4occt interactive application
│   ├── examples/
│   │   ├── B1/                              # water phantom example
│   │   ├── B4c/                             # sampling calorimeter example
│   │   └── DD4hepSimpleDetector/            # DD4hep STEP-backed VXD layer example
│   ├── dd4hep/                              # optional DD4hep detector element plugins
│   │   ├── G4OCCT_STEPSolid.cc
│   │   ├── G4OCCT_STEPAssembly.cc
│   │   └── G4OCCT_STEPAssemblySD.cc
│   ├── tests/                               # CTest-integrated unit tests
│   └── benchmarks/                          # navigator benchmarks
└── docs/
    ├── goals.md
    ├── geometry_mapping.md
    ├── reference_position.md
    ├── solid_navigation.md
    ├── performance.md
    ├── low_level_optimization.md
    ├── material_bridging.md
    ├── step_assembly_import.md
    ├── dd4hep_plugin.md
    ├── geometry_test_status.md
    ├── nist_ctc.md
    ├── example_b1.md
    ├── example_b4c.md
    ├── example_dd4hep_simpledetector.md
    └── slides.html
```

---

## Roadmap

| Milestone | Status | Description | Tracking |
|---|---|---|---|
| v0.1 | ✅ Complete | CMake skeleton, stub classes, CI, docs | — |
| v0.2 | ✅ Complete | `Inside` via multi-stage pipeline (inscribed sphere + ray-parity + classifier fallback) | [Solid Navigation Design](solid_navigation.md) §2.1 |
| v0.3 | ✅ Complete | `DistanceToIn/Out` via per-face `IntCurvesFace_Intersector` loop with AABB prefilter | [Solid Navigation Design](solid_navigation.md) §2.3–2.6, [Performance Considerations](performance.md) |
| v0.4 | ✅ Complete | STEP import end-to-end example | [Example B1 — Water Phantom](example_b1.md) |
| v0.5 | 🔨 In progress | Full test suite passing for all G4 primitives | [Geometry Test Status](geometry_test_status.md) |
| v0.6 | ✅ Complete | Multi-shape STEP assembly import (`G4OCCTAssemblyVolume`) | [Multi-Shape STEP Assembly Import](step_assembly_import.md) |
| v0.7 | ✅ Complete | DD4hep plugin (`G4OCCT_STEPSolid`, `G4OCCT_STEPAssembly`, `G4OCCT_STEPAssemblySD`) | [DD4hep Plugin Design](dd4hep_plugin.md) |
| v0.8 | ✅ Complete | Material bridging (`G4OCCTMaterialMap`, `G4OCCTMaterialMapReader`) | [Material Bridging](material_bridging.md) |
| v0.9 | ✅ Complete | Sensitive detector mapping (`G4OCCTSensitiveDetectorMap/Reader`, `G4OCCTAssemblyRegistry`) | — |
| v0.10 | ✅ Complete | ROOT/TGeo adapter (`TGeoOCCTSolid`, `BUILD_ROOT_TGEO_SUPPORT`) and standalone `g4occt` application | — |
| v1.0 | 🔲 Planned | Production-quality performance | [Performance Considerations](performance.md) |
