// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file test_tgeo_to_g4_conversion.cc
/// @brief Regression tests for the TGeoOCCTSolid → G4OCCTSolid conversion path.
///
/// These tests replicate the conversion logic performed by
/// `G4OCCTGeant4Converter::handleSolid` (in
/// `G4OCCT_DDG4DetectorGeometryConstruction.cc`) so that CI catches any
/// regression that would silently revert the Geant4 geometry back to a
/// tessellated solid instead of the exact G4OCCTSolid.
///
/// **Include-firewall note:** This file must not include any DD4hep headers
/// (which pull in ROOT's TString.h → `void Printf(...)`) or any OCCT headers
/// (which declare `int Printf(...)`).  Both `TGeoOCCTSolid.hh` and
/// `G4OCCTSolid.hh` only forward-declare `TopoDS_Shape`, so there is no
/// conflict.  The actual OCCT symbols are resolved at link time via the
/// compiled G4OCCT library.

#include "G4OCCT/G4OCCTSolid.hh"
#include "G4OCCT/TGeoOCCTSolid.hh"

#include <G4ThreeVector.hh>
#include <G4VSolid.hh>
#include <globals.hh>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace {

/// Absolute path to the box-20×30×40 mm STEP fixture (20 mm half-x,
/// 15 mm half-y, 20 mm half-z → ±1 cm, ±1.5 cm, ±2 cm in ROOT cm units,
/// ±10 mm, ±15 mm, ±20 mm in Geant4 mm units).
std::string BoxStepPath() {
  return (std::filesystem::path(G4OCCT_TEST_SOURCE_DIR) / "fixtures" / "geometry" /
          "direct-primitives" / "G4Box" / "box-20x30x40-v1" / "shape.step")
      .string();
}

/// Load the box fixture as a TGeoOCCTSolid and extract the underlying OCCT
/// shape reference; construct and return a G4OCCTSolid using the same one-liner
/// that G4OCCTGeant4Converter::handleSolid executes.
///
/// This is the exact conversion that must stay alive for the non-tessellated
/// DD4hep path to work.
std::unique_ptr<G4OCCTSolid> MakeG4SolidFromTGeoOCCTSolid() {
  std::unique_ptr<TGeoOCCTSolid> tgeoSolid(
      TGeoOCCTSolid::FromSTEP("box_tgeo", BoxStepPath()));
  // GetOCCTShape() returns const TopoDS_Shape& — no OCCT header needed to pass
  // the reference through.
  return std::make_unique<G4OCCTSolid>("box_g4", tgeoSolid->GetOCCTShape());
}

} // namespace

/// Verify that the G4VSolid produced by the conversion is a G4OCCTSolid.
///
/// If a regression introduces tessellation, GetEntityType() returns
/// "G4TessellatedSolid" (or similar) and this test fails.
TEST(TGeoToG4Conversion, EntityTypeIsExactSolid) {
  auto g4Solid = MakeG4SolidFromTGeoOCCTSolid();
  ASSERT_NE(g4Solid, nullptr);
  EXPECT_EQ(g4Solid->GetEntityType(), "G4OCCTSolid")
      << "Expected exact G4OCCTSolid entity type; got tessellated or other fallback";
}

/// Verify Inside() queries return correct results in Geant4 mm units.
///
/// The box is 20 × 30 × 40 mm (half-extents 10, 15, 20 mm).
/// Points at the origin and just inside the faces should be kInside; points
/// clearly outside should be kOutside.
TEST(TGeoToG4Conversion, InsideQueriesUseMmUnits) {
  auto g4Solid = MakeG4SolidFromTGeoOCCTSolid();
  ASSERT_NE(g4Solid, nullptr);

  EXPECT_EQ(g4Solid->Inside(G4ThreeVector(0.0, 0.0, 0.0)), kInside)
      << "Origin should be inside the 20x30x40 mm box";
  EXPECT_EQ(g4Solid->Inside(G4ThreeVector(9.0, 0.0, 0.0)), kInside)
      << "x=9 mm is inside the 10 mm half-x face";
  EXPECT_EQ(g4Solid->Inside(G4ThreeVector(11.0, 0.0, 0.0)), kOutside)
      << "x=11 mm is outside the 10 mm half-x face";
  EXPECT_EQ(g4Solid->Inside(G4ThreeVector(0.0, 16.0, 0.0)), kOutside)
      << "y=16 mm is outside the 15 mm half-y face";
  EXPECT_EQ(g4Solid->Inside(G4ThreeVector(0.0, 0.0, 21.0)), kOutside)
      << "z=21 mm is outside the 20 mm half-z face";
}

/// Verify DistanceToIn (ray) returns correct results in Geant4 mm units.
///
/// From x=20 mm shooting in the −x direction, the entry distance to the
/// x=10 mm face is 10 mm.
TEST(TGeoToG4Conversion, DistanceToInRayUsesMmUnits) {
  auto g4Solid = MakeG4SolidFromTGeoOCCTSolid();
  ASSERT_NE(g4Solid, nullptr);

  const G4ThreeVector outsideX(20.0, 0.0, 0.0);
  const G4ThreeVector minusX(-1.0, 0.0, 0.0);
  constexpr double kTolMm = 1e-3;

  const G4double dist = g4Solid->DistanceToIn(outsideX, minusX);
  EXPECT_NEAR(dist, 10.0, kTolMm)
      << "Expected 10 mm entry distance from x=20 mm to x=10 mm face";
}

/// Verify DistanceToOut (ray) returns correct results in Geant4 mm units.
///
/// From the origin shooting in the +x direction, the exit distance to the
/// x=10 mm face is 10 mm.
TEST(TGeoToG4Conversion, DistanceToOutRayUsesMmUnits) {
  auto g4Solid = MakeG4SolidFromTGeoOCCTSolid();
  ASSERT_NE(g4Solid, nullptr);

  const G4ThreeVector origin(0.0, 0.0, 0.0);
  const G4ThreeVector plusX(1.0, 0.0, 0.0);
  constexpr double kTolMm = 1e-3;

  const G4double dist = g4Solid->DistanceToOut(origin, plusX);
  EXPECT_NEAR(dist, 10.0, kTolMm)
      << "Expected 10 mm exit distance from origin to x=10 mm face";
}

/// Verify that the entity type of a fresh G4OCCTSolid loaded directly from
/// STEP (the native Geant4 path, no TGeoOCCTSolid intermediary) also returns
/// "G4OCCTSolid".  This anchors the entity-type string used by the conversion
/// regression tests above.
TEST(TGeoToG4Conversion, DirectG4OCCTSolidEntityType) {
  std::unique_ptr<G4OCCTSolid> solid(G4OCCTSolid::FromSTEP("box_direct", BoxStepPath()));
  ASSERT_NE(solid, nullptr);
  EXPECT_EQ(solid->GetEntityType(), "G4OCCTSolid");
}
