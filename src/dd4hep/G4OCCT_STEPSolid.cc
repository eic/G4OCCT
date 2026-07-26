// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file G4OCCT_STEPSolid.cc
/// @brief DD4hep detector-element plugin: single solid from a STEP file.
///
/// Compact XML usage:
/// @code{.xml}
/// <detector id="1" name="MyBeamPipe" type="G4OCCT_STEPSolid" vis="BlueVis">
///   <step_file path="geometry/beampipe.step"/>
///   <position x="0" y="0" z="0"/>
///   <rotation x="0" y="0" z="0"/>
///   <material name="Aluminium"/>
/// </detector>
/// @endcode
///
/// The implementation constructs a @c TGeoOCCTSolid directly from STEP.  This
/// keeps DD4hep/ROOT geometry and navigation on the exact shared G4OCCT query
/// kernel instead of a tessellated approximation.
#include <DD4hep/DetFactoryHelper.h>
#include <DD4hep/Printout.h>
#include "G4OCCT/TGeoOCCTSolid.hh"

#include <stdexcept>
#include <string>

using namespace dd4hep;

static Ref_t create_step_solid(Detector& description, xml_h e, SensitiveDetector /*sens*/) {
  xml_det_t x_det   = e;
  xml_comp_t x_step = x_det.child(_Unicode(step_file));
  xml_comp_t x_pos  = x_det.child(_Unicode(position));
  xml_comp_t x_rot  = x_det.child(_Unicode(rotation));
  xml_comp_t x_mat  = x_det.child(_Unicode(material));

  std::string name = x_det.nameStr();
  std::string path = x_step.attr<std::string>(_Unicode(path));

  TGeoOCCTSolid* stepSolid = nullptr;
  try {
    stepSolid = TGeoOCCTSolid::FromSTEP(name.c_str(), path);
  } catch (const std::exception& ex) {
    throw std::runtime_error("G4OCCT_STEPSolid: failed to import '" + path + "' (" + ex.what() +
                             ")");
  }
  if (!stepSolid) {
    throw std::runtime_error("G4OCCT_STEPSolid: import of '" + path +
                             "' returned a null TGeoOCCTSolid");
  }
  printout(INFO, "G4OCCT_STEPSolid", "Imported '%s' from '%s' as exact TGeoOCCTSolid",
           name.c_str(), path.c_str());

  // ── DD4hep volume and placement ──────────────────────────────────────────
  Material mat = description.material(x_mat.attr<std::string>(_Unicode(name)));
  Volume vol(name + "_vol", Solid(stepSolid), mat);
  vol.setVisAttributes(description, x_det.visStr());

  DetElement det(name, x_det.id());
  Position pos(x_pos.x(), x_pos.y(), x_pos.z());
  RotationZYX rot(x_rot.z(), x_rot.y(), x_rot.x());
  PlacedVolume pv = description.pickMotherVolume(det).placeVolume(vol, Transform3D(rot, pos));
  pv.addPhysVolID("system", x_det.id());
  det.setPlacement(pv);
  return det;
}

DECLARE_DETELEMENT(G4OCCT_STEPSolid, create_step_solid)
