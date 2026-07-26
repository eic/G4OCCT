// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file G4OCCT_DDG4DetectorGeometryConstruction.cc
/// @brief DDG4 detector-construction action preserving exact G4OCCT solids.

#include <DD4hep/DetElement.h>
#include <DD4hep/Printout.h>

#include <DDG4/Factories.h>
#include <DDG4/Geant4Converter.h>
#include <DDG4/Geant4DetectorConstruction.h>
#include <DDG4/Geant4HierarchyDump.h>
#include <DDG4/Geant4Kernel.h>
#include <DDG4/Geant4Mapping.h>

#include <G4VPhysicalVolume.hh>

#include "G4OCCT/G4OCCTSolid.hh"
#include "G4OCCT/TGeoOCCTSolid.hh"

namespace dd4hep {
namespace sim {

  /// Geant4 converter extension that maps TGeoOCCTSolid -> exact G4OCCTSolid.
  class G4OCCTGeant4Converter final : public Geant4Converter {
  public:
    using Geant4Converter::Geant4Converter;

    void* handleSolid(const std::string& name, const TGeoShape* shape) const override {
      if (shape && shape->IsA() == TGeoOCCTSolid::Class()) {
        if (G4VSolid* cached = data().g4Solids[shape]) {
          return cached;
        }
        auto* occtShape        = static_cast<const TGeoOCCTSolid*>(shape);
        auto* exact            = new G4OCCTSolid(name.c_str(), occtShape->GetOCCTShape());
        data().g4Solids[shape] = exact;
        printout(outputLevel, "G4OCCTGeant4Converter",
                 "Converted TGeoOCCTSolid '%s' to exact G4OCCTSolid", name.c_str());
        return exact;
      }
      return Geant4Converter::handleSolid(name, shape);
    }
  };

  /// DDG4 geometry-construction action using G4OCCTGeant4Converter.
  class G4OCCTDetectorGeometryConstruction final : public Geant4DetectorConstruction {
    unsigned long m_dumpHierarchy{0};
    long m_debugVolManager{0};
    bool m_haveVolManager{true};

    bool m_debugMaterials{false};
    bool m_debugElements{false};
    bool m_debugShapes{false};
    bool m_debugVolumes{false};
    bool m_debugPlacements{false};
    bool m_debugReflections{false};
    bool m_debugRegions{false};
    bool m_debugLimits{false};
    bool m_debugSurfaces{false};

    bool m_printPlacements{false};
    bool m_printSensitives{false};

    int m_geoInfoPrintLevel{DEBUG};
    std::string m_dumpGDML{};

  public:
    G4OCCTDetectorGeometryConstruction(Geant4Context* ctxt, const std::string& nam)
        : Geant4DetectorConstruction(ctxt, nam) {
      declareProperty("DebugMaterials", m_debugMaterials);
      declareProperty("DebugElements", m_debugElements);
      declareProperty("DebugShapes", m_debugShapes);
      declareProperty("DebugVolumes", m_debugVolumes);
      declareProperty("DebugPlacements", m_debugPlacements);
      declareProperty("DebugReflections", m_debugReflections);
      declareProperty("DebugRegions", m_debugRegions);
      declareProperty("DebugLimits", m_debugLimits);
      declareProperty("DebugSurfaces", m_debugSurfaces);
      declareProperty("DebugVolManager", m_debugVolManager);
      declareProperty("HaveVolManager", m_haveVolManager);
      declareProperty("PrintPlacements", m_printPlacements);
      declareProperty("PrintSensitives", m_printSensitives);
      declareProperty("GeoInfoPrintLevel", m_geoInfoPrintLevel);
      declareProperty("DumpHierarchy", m_dumpHierarchy);
      declareProperty("DumpGDML", m_dumpGDML);
    }

    void constructGeo(Geant4DetectorConstructionContext* ctxt) override {
      Geant4Mapping& g4map = Geant4Mapping::instance();
      DetElement world     = ctxt->description.world();

      G4OCCTGeant4Converter conv(ctxt->description, outputLevel());
      conv.debugMaterials   = m_debugMaterials;
      conv.debugElements    = m_debugElements;
      conv.debugShapes      = m_debugShapes;
      conv.debugVolumes     = m_debugVolumes;
      conv.debugRegions     = m_debugRegions;
      conv.debugSurfaces    = m_debugSurfaces;
      conv.debugPlacements  = m_debugPlacements;
      conv.debugReflections = m_debugReflections;
      conv.debugLimits      = m_debugLimits;
      conv.printPlacements  = m_printPlacements;
      conv.printSensitives  = m_printSensitives;

      ctxt->geometry             = conv.create(world).detach();
      ctxt->geometry->printLevel = outputLevel();
      g4map.attach(ctxt->geometry);

      G4VPhysicalVolume* w = ctxt->geometry->world();
      context()->kernel().setWorld(w);

      g4map.debugVolManager = m_debugVolManager;
      g4map.haveVolManager  = m_haveVolManager;
      if (m_haveVolManager) {
        g4map.volumeManager();
      }
      if (m_dumpHierarchy != 0) {
        Geant4HierarchyDump dmp(ctxt->description, m_dumpHierarchy);
        dmp.dump("", w);
      }
      ctxt->world = w;
    }
  };

} // namespace sim
} // namespace dd4hep

using namespace dd4hep::sim;
DECLARE_GEANT4ACTION(G4OCCTDetectorGeometryConstruction)
