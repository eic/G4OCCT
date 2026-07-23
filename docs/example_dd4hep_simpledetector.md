<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
<!-- Copyright (C) 2026 G4OCCT Contributors -->

# Example: DD4hep SimpleDetector — STEP-Backed VXD Layer

The `src/examples/DD4hepSimpleDetector` directory ports the first VXD barrel
layer from the standard DD4hep `SimpleDetector` example (`Simple_ILD.xml`) into
G4OCCT. Instead of letting `ZPlanarTracker` synthesize ladder and sensor boxes
from XML dimensions, this example loads those shapes from STEP files through the
`G4OCCT_STEPSolid` DD4hep plugin.

---

## What the Example Does

The example builds a minimal DD4hep compact geometry containing:

| Geometry | Source | Material |
|---|---|---|
| 10 support ladders | `step/support_ladder.step` | `CarbonFiber` |
| 10 silicon ladders | `step/sensor_ladder.step` | `Silicon` |

The 20 detector elements are arranged in the same 10-fold barrel pattern as the
first `Simple_ILD` VXD layer, using the original `phi0 = -90 deg` layout and
layer radii derived from the DD4hep example.

---

## Building the Example

Configure with both examples and the DD4hep plugin enabled:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_DD4HEP_PLUGIN=ON
cmake --build build -- -j$(nproc)
```

The executable is placed at:

```text
build/src/examples/DD4hepSimpleDetector/exampleDD4hepSimpleDetector
```

---

## Running the Example

Run with the configured compact file baked in:

```bash
./build/src/examples/DD4hepSimpleDetector/exampleDD4hepSimpleDetector
```

Or point it at the generated compact XML explicitly:

```bash
./build/src/examples/DD4hepSimpleDetector/exampleDD4hepSimpleDetector \
  build/src/examples/DD4hepSimpleDetector/Simple_ILD_G4OCCT.xml
```

On success it prints a summary confirming that all 10 support ladders and all
10 silicon ladders were loaded from STEP-backed detector elements.

---

## Key Files

| File | Purpose |
|---|---|
| `exampleDD4hepSimpleDetector.cc` | Minimal DD4hep loader that validates the compact geometry |
| `compact/Simple_ILD_G4OCCT.xml.in` | Compact XML template derived from the first `Simple_ILD` VXD layer |
| `step/support_ladder.step` | STEP box for the carbon-fiber support ladder |
| `step/sensor_ladder.step` | STEP box for the silicon sensor ladder |
| `step/generate_step_boxes.py` | Regenerates both STEP files from the original ladder dimensions |

---

## What Was Ported from SimpleDetector

The standard DD4hep `Simple_ILD.xml` example defines the first VXD layer as a
10-ladder `ZPlanarTracker` barrel with:

- support ladders: `11.5 x 62.5 x 1.0 mm`
- silicon ladders: `11.0 x 62.5 x 0.05 mm`
- azimuthal pattern: `phi0 = -90 deg`, `nLadders = 10`

This G4OCCT example preserves that layer topology while replacing the
parametric boxes with STEP solids imported by the DD4hep plugin.
