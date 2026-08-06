// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file main.cc
/// @brief step2dd4hep — generate a DD4hep compact file from a STEP geometry.
///
/// Usage:
/// @code
/// step2dd4hep [options] geometry.step
///
/// Options:
///   -o <file>        Output compact XML (default: <stem>.xml)
///   -m <file>        Output material-map XML (default: <stem>_materials.xml)
///   --existing-map <file>
///                    Existing material-map XML to read; only unmapped STEP
///                    material names will be added to the generated stub.
///   --detector-id <n>
///                    Numeric detector id in the compact file (default: 1)
///   --detector-name <name>
///                    Detector element name (default: derived from STEP stem)
///   -h, --help       Print this help
/// @endcode
///
/// The tool reads the STEP file with the OpenCASCADE XDE framework, collects
/// all unique material names attached to solid labels (the same attribute that
/// G4OCCTAssemblyVolume::FromSTEP uses), and emits:
///
///  1. A minimal DD4hep compact XML that references the STEP file and the
///     generated material-map via the G4OCCT_STEPAssembly plugin.
///  2. A material-map XML stub with one @c \<entry\> per unique STEP material
///     name, with @c dd4hep_material left as a TODO placeholder (or filled in
///     from an existing map that the user provides).
///
/// If no material names are found in the STEP metadata, the label names of the
/// free shapes are used instead, which matches the fall-back key used by
/// G4OCCTAssemblyVolume when a material attribute is absent.

#include <IFSelect_ReturnStatus.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── OCCT helpers ─────────────────────────────────────────────────────────────

/// Return the TDataStd_Name value for @p label, or empty string.
static std::string LabelName(const TDF_Label& label) {
  Handle(TDataStd_Name) nameAttr;
  if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
    TCollection_ExtendedString ext = nameAttr->Get();
    TCollection_AsciiString ascii(ext, '?');
    return std::string(ascii.ToCString());
  }
  return {};
}

/// Return the XDE material name for @p label, or empty string.
static std::string MaterialName(const TDF_Label& label,
                                const Handle(XCAFDoc_MaterialTool) & matTool) {
  Handle(TCollection_HAsciiString) matName;
  Handle(TCollection_HAsciiString) matDescription;
  Standard_Real density{};
  Handle(TCollection_HAsciiString) densityName;
  Handle(TCollection_HAsciiString) densityValType;
  if (matTool->GetMaterial(label, matName, matDescription, density, densityName, densityValType)) {
    if (!matName.IsNull())
      return std::string(matName->ToCString());
  }
  return {};
}

/// Recursively walk the XDE label tree and collect unique material keys.
///
/// The priority order matches G4OCCTAssemblyVolume::ImportLabel:
///  1. XDE material attribute on the referred shape label.
///  2. TDataStd_Name of the referred shape label.
static void CollectMaterials(const TDF_Label& label, const Handle(XCAFDoc_ShapeTool) & shapeTool,
                             const Handle(XCAFDoc_MaterialTool) & matTool,
                             std::set<std::string>& out) {
  if (shapeTool->IsAssembly(label)) {
    TDF_LabelSequence components;
    shapeTool->GetComponents(label, components, /*recursive=*/Standard_False);
    for (Standard_Integer i = 1; i <= components.Length(); ++i) {
      TDF_Label referred;
      if (shapeTool->GetReferredShape(components.Value(i), referred))
        CollectMaterials(referred, shapeTool, matTool, out);
      else
        CollectMaterials(components.Value(i), shapeTool, matTool, out);
    }
    return;
  }

  // Solid label: try material attribute first, then label name.
  std::string key = MaterialName(label, matTool);
  if (key.empty())
    key = LabelName(label);
  if (!key.empty())
    out.insert(key);
}

// ── XML helpers ───────────────────────────────────────────────────────────────

/// Escape characters that must be escaped inside an XML attribute value.
static std::string XmlAttr(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    default:
      out += c;
    }
  }
  return out;
}

// ── Existing material map reader (lightweight, no Geant4 dependency) ─────────

/// Unescape the five predefined XML entities in an attribute value string
/// so that names read back from the written XML match the raw OCCT strings.
static std::string XmlUnescape(std::string s) {
  struct Ent {
    const char* escaped;
    const char* raw;
  };
  static constexpr Ent kEntities[] = {
      {"&amp;", "&"}, {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&apos;", "'"},
  };
  for (const auto& e : kEntities) {
    std::string::size_type pos = 0;
    while ((pos = s.find(e.escaped, pos)) != std::string::npos) {
      s.replace(pos, std::strlen(e.escaped), e.raw);
      pos += std::strlen(e.raw);
    }
  }
  return s;
}

/// Parse an existing material-map XML produced by this tool and return a map
/// of step_name → dd4hep_material for entries that are not placeholders.
static std::map<std::string, std::string> ReadExistingMap(const std::string& path) {
  std::map<std::string, std::string> result;
  std::ifstream in(path);
  if (!in.is_open()) {
    std::cerr << "step2dd4hep: warning: cannot open existing map '" << path << "' — ignored\n";
    return result;
  }
  std::string line;
  while (std::getline(in, line)) {
    // Look for:  <entry step_name="..." dd4hep_material="..."/>
    auto spos = line.find("step_name=\"");
    auto mpos = line.find("dd4hep_material=\"");
    if (spos == std::string::npos || mpos == std::string::npos)
      continue;
    spos += 11;
    auto send = line.find('"', spos);
    mpos += 17;
    auto mend = line.find('"', mpos);
    if (send == std::string::npos || mend == std::string::npos)
      continue;
    std::string stepName = XmlUnescape(line.substr(spos, send - spos));
    std::string matName  = XmlUnescape(line.substr(mpos, mend - mpos));
    if (!matName.empty() && matName != "TODO")
      result[stepName] = matName;
  }
  return result;
}

// ── Usage ─────────────────────────────────────────────────────────────────────

static void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " [options] geometry.step\n"
            << "\n"
            << "Options:\n"
            << "  -o <file>              Output compact XML (default: ./<stem>.xml)\n"
            << "  -m <file>              Output material-map XML "
               "(default: ./<stem>_materials.xml)\n"
            << "  --existing-map <file>  Existing material-map XML to seed mappings from\n"
            << "  --detector-id <n>      Detector id in compact file (default: 1)\n"
            << "  --detector-name <n>    Detector element name (default: STEP file stem)\n"
            << "  -h, --help             Print this help\n";
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  std::string stepPath;
  std::string compactOut;
  std::string materialOut;
  std::string existingMapPath;
  std::string detectorName;
  int detectorId = 1;

  // ── Argument parsing ──────────────────────────────────────────────────────
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg == "-o") {
      if (++i >= argc) {
        std::cerr << "step2dd4hep: -o requires an argument\n";
        return 1;
      }
      compactOut = argv[i];
    } else if (arg == "-m") {
      if (++i >= argc) {
        std::cerr << "step2dd4hep: -m requires an argument\n";
        return 1;
      }
      materialOut = argv[i];
    } else if (arg == "--existing-map") {
      if (++i >= argc) {
        std::cerr << "step2dd4hep: --existing-map requires an argument\n";
        return 1;
      }
      existingMapPath = argv[i];
    } else if (arg == "--detector-id") {
      if (++i >= argc) {
        std::cerr << "step2dd4hep: --detector-id requires an argument\n";
        return 1;
      }
      try {
        detectorId = std::stoi(argv[i]);
      } catch (const std::exception& e) {
        std::cerr << "step2dd4hep: --detector-id requires a valid integer, got '" << argv[i]
                  << "': " << e.what() << "\n";
        return 1;
      }
    } else if (arg == "--detector-name") {
      if (++i >= argc) {
        std::cerr << "step2dd4hep: --detector-name requires an argument\n";
        return 1;
      }
      detectorName = argv[i];
    } else if (arg[0] == '-') {
      std::cerr << "step2dd4hep: unknown option '" << arg << "'\n";
      PrintUsage(argv[0]);
      return 1;
    } else {
      if (!stepPath.empty()) {
        std::cerr << "step2dd4hep: multiple STEP files specified; only one is supported\n";
        return 1;
      }
      stepPath = arg;
    }
  }

  if (stepPath.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  // ── Derive default output paths ───────────────────────────────────────────
  fs::path stepFsPath(stepPath);
  std::string stem = stepFsPath.stem().string();

  if (compactOut.empty())
    compactOut = (fs::current_path() / (stem + ".xml")).string();
  if (materialOut.empty())
    materialOut = (fs::current_path() / (stem + "_materials.xml")).string();
  if (detectorName.empty())
    detectorName = stem;

  // ── Read STEP file with OCCT XDE ──────────────────────────────────────────
  Handle(TDocStd_Application) app = new TDocStd_Application;
  Handle(TDocStd_Document) doc;
  app->NewDocument("MDTV-CAF", doc);

  STEPCAFControl_Reader reader;
  reader.SetNameMode(Standard_True);
  reader.SetMatMode(Standard_True);

  IFSelect_ReturnStatus status = reader.ReadFile(stepPath.c_str());
  if (status != IFSelect_RetDone) {
    std::cerr << "step2dd4hep: failed to read STEP file '" << stepPath << "'\n";
    return 1;
  }
  if (!reader.Transfer(doc)) {
    std::cerr << "step2dd4hep: STEP transfer to XDE document failed\n";
    return 1;
  }

  Handle(XCAFDoc_ShapeTool) shapeTool  = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  Handle(XCAFDoc_MaterialTool) matTool = XCAFDoc_DocumentTool::MaterialTool(doc->Main());

  // ── Collect unique material/shape keys ────────────────────────────────────
  TDF_LabelSequence freeShapes;
  shapeTool->GetFreeShapes(freeShapes);

  std::set<std::string> stepNames;
  for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i)
    CollectMaterials(freeShapes.Value(i), shapeTool, matTool, stepNames);

  if (stepNames.empty()) {
    std::cerr << "step2dd4hep: warning: no material or label names found in '" << stepPath
              << "'; the material map will be empty\n";
  }

  // ── Load existing map (if any) ────────────────────────────────────────────
  std::map<std::string, std::string> knownMappings;
  if (!existingMapPath.empty())
    knownMappings = ReadExistingMap(existingMapPath);

  // ── Generate material-map XML ─────────────────────────────────────────────
  {
    std::ofstream out(materialOut);
    if (!out.is_open()) {
      std::cerr << "step2dd4hep: cannot write material map to '" << materialOut << "'\n";
      return 1;
    }
    out << "<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->\n"
        << "<!-- Copyright (C) 2026 G4OCCT Contributors -->\n"
        << "<!--\n"
        << "  Material map for " << stem << "\n"
        << "  Generated by step2dd4hep from: " << stepPath << "\n"
        << "\n"
        << "  For each <entry>:\n"
        << "    step_name       — the name that appears in the STEP file\n"
        << "    dd4hep_material — the DD4hep / Geant4 NIST material name\n"
        << "                      (e.g. \"G4_Al\", \"G4_Fe\", \"G4_AIR\", …)\n"
        << "  Replace every \"TODO\" placeholder with the correct material name.\n"
        << "-->\n"
        << "<material_map>\n";
    for (const auto& name : stepNames) {
      auto it            = knownMappings.find(name);
      std::string mapped = (it != knownMappings.end()) ? it->second : "TODO";
      out << "  <entry step_name=\"" << XmlAttr(name) << "\""
          << " dd4hep_material=\"" << XmlAttr(mapped) << "\"/>\n";
    }
    out << "</material_map>\n";
  }
  std::cout << "step2dd4hep: wrote material map → " << materialOut << "\n";

  // ── Generate compact XML ──────────────────────────────────────────────────
  // Use an absolute path so the compact file works regardless of cwd.
  fs::path absStep = fs::absolute(stepFsPath);
  fs::path absMat  = fs::absolute(fs::path(materialOut));

  {
    std::ofstream out(compactOut);
    if (!out.is_open()) {
      std::cerr << "step2dd4hep: cannot write compact XML to '" << compactOut << "'\n";
      return 1;
    }
    out << "<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->\n"
        << "<!-- Copyright (C) 2026 G4OCCT Contributors -->\n"
        << "<!--\n"
        << "  DD4hep compact file for " << stem << "\n"
        << "  Generated by step2dd4hep from: " << stepPath << "\n"
        << "\n"
        << "  Fill in the material map (" << absMat.filename().string() << ")\n"
        << "  before running ddsim or npsim.\n"
        << "-->\n"
        << "<lccdd>\n"
        << "  <info name=\"" << XmlAttr(stem) << "\"\n"
        << "        title=\"" << XmlAttr(stem) << "\"\n"
        << "        author=\"\"\n"
        << "        url=\"\"\n"
        << "        status=\"development\"\n"
        << "        version=\"0.0\">\n"
        << "    <comment>Compact geometry generated by step2dd4hep.</comment>\n"
        << "  </info>\n"
        << "\n"
        << "  <define>\n"
        << "    <constant name=\"world_side\"         value=\"10000*mm\"/>\n"
        << "    <constant name=\"world_x\"             value=\"world_side/2\"/>\n"
        << "    <constant name=\"world_y\"             value=\"world_side/2\"/>\n"
        << "    <constant name=\"world_z\"             value=\"world_side/2\"/>\n"
        << "    <constant name=\"tracker_region_rmax\" value=\"world_side/2\"/>\n"
        << "    <constant name=\"tracker_region_zmax\" value=\"world_side/2\"/>\n"
        << "  </define>\n"
        << "\n"
        << "  <detectors>\n"
        << "    <detector id=\"" << detectorId << "\"\n"
        << "              name=\"" << XmlAttr(detectorName) << "\"\n"
        << "              type=\"G4OCCT_STEPAssembly\">\n"
        << "      <step_file path=\"" << XmlAttr(absStep.string()) << "\"/>\n"
        << "      <position x=\"0\" y=\"0\" z=\"0\"/>\n"
        << "      <material_map>\n"
        << "        <!--\n"
        << "          Auto-generated from STEP material/label names.\n"
        << "          Replace TODO values in " << absMat.filename().string() << "\n"
        << "          and re-run step2dd4hep, or edit the entries below directly.\n"
        << "        -->\n";

    for (const auto& name : stepNames) {
      auto it            = knownMappings.find(name);
      std::string mapped = (it != knownMappings.end()) ? it->second : "TODO";
      out << "        <entry step_name=\"" << XmlAttr(name) << "\""
          << " dd4hep_material=\"" << XmlAttr(mapped) << "\"/>\n";
    }

    out << "      </material_map>\n"
        << "    </detector>\n"
        << "  </detectors>\n"
        << "\n"
        << "</lccdd>\n";
  }
  std::cout << "step2dd4hep: wrote compact XML    → " << compactOut << "\n";

  if (!stepNames.empty()) {
    long nTodo = std::count_if(stepNames.begin(), stepNames.end(), [&](const std::string& n) {
      return knownMappings.find(n) == knownMappings.end();
    });
    if (nTodo > 0)
      std::cout << "step2dd4hep: " << nTodo << " material(s) need mapping in "
                << absMat.filename().string() << " (marked TODO)\n";
  }

  return 0;
}
