<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
<!-- Copyright (C) 2026 G4OCCT Contributors -->

# step2dd4hep — STEP to DD4hep compact generator

`step2dd4hep` is a small command-line tool that reads a STEP geometry file,
extracts all unique material and shape names embedded in the file's XDE
metadata, and generates:

1. A **DD4hep compact XML** file that references the STEP file and wires it to
   the `G4OCCT_STEPAssembly` detector plugin.
2. A **material-map XML** stub with one entry per unique STEP material name.
   Entries whose target Geant4/DD4hep material is not yet known are marked
   `TODO` for the user to fill in.

## Quick start

```bash
# Generate compact.xml and compact_materials.xml from geometry.stp
step2dd4hep geometry.stp

# With an existing partial material map
step2dd4hep --existing-map old_materials.xml geometry.stp

# Custom output names
step2dd4hep -o detector.xml -m detector_mat.xml geometry.stp
```

After running the tool, open `<stem>_materials.xml`, replace every `TODO`
value with the correct Geant4 NIST material name (e.g. `G4_Al`, `G4_Fe`,
`G4_AIR`), then pass the compact file to `ddsim` or `npsim`:

```bash
ddsim --compactFile geometry.xml --runType run -N 1000
```

## Command-line reference

```text
step2dd4hep [options] geometry.step

Options:
  -o <file>              Output compact XML (default: ./<stem>.xml)
  -m <file>              Output material-map XML (default: ./<stem>_materials.xml)
  --existing-map <file>  Existing material-map XML to seed known mappings from
  --detector-id <n>      Numeric detector id in the compact file (default: 1)
  --detector-name <n>    Detector element name (default: STEP file stem)
  -h, --help             Print help
```

## How material names are resolved

The tool uses the same priority order as `G4OCCTAssemblyVolume::FromSTEP`:

1. **XDE material attribute** (`XCAFDoc_MaterialTool`) attached to the solid
   label — the preferred source when the CAD tool exports material metadata.
2. **`TDataStd_Name`** label name of the referred solid label — used when no
   material attribute is present.

Each unique name found becomes one `<entry>` in the material map.

## Output format

### Compact XML

```xml
<lccdd>
  ...
  <detectors>
    <detector id="1" name="MyDetector" type="G4OCCT_STEPAssembly">
      <step_file path="/abs/path/to/geometry.stp"/>
      <position x="0" y="0" z="0"/>
      <material_map>
        <entry step_name="Lead"   dd4hep_material="G4_Pb"/>
        <entry step_name="Copper" dd4hep_material="TODO"/>
      </material_map>
    </detector>
  </detectors>
</lccdd>
```

### Material-map XML

```xml
<material_map>
  <entry step_name="Lead"   dd4hep_material="G4_Pb"/>
  <entry step_name="Copper" dd4hep_material="TODO"/>
</material_map>
```

> **Note:** The compact XML embeds the material map inline for convenience.
> When you update `_materials.xml` and re-run `step2dd4hep`, the compact XML
> is regenerated with the updated mappings.  Alternatively, edit the compact
> XML directly.

## See also

- [DD4hep plugin reference](dd4hep_plugin.md)
- [Material bridging strategy](material_bridging.md)
