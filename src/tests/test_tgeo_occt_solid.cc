// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include "G4OCCT/TGeoOCCTSolid.hh"

#include <TGeoShape.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace {

std::filesystem::path FixtureFamilyPath(const char* family) {
  return std::filesystem::path(G4OCCT_TEST_SOURCE_DIR) / "fixtures" / "geometry" / family;
}

std::unique_ptr<TGeoOCCTSolid> LoadBox() {
  return std::unique_ptr<TGeoOCCTSolid>(TGeoOCCTSolid::FromSTEP(
      "box", (FixtureFamilyPath("direct-primitives") / "G4Box" / "box-20x30x40-v1" / "shape.step")
                 .string()));
}

std::unique_ptr<TGeoOCCTSolid> LoadSphere() {
  return std::unique_ptr<TGeoOCCTSolid>(TGeoOCCTSolid::FromSTEP(
      "sphere",
      (FixtureFamilyPath("direct-primitives") / "G4Sphere" / "sphere-r15-v1" / "shape.step")
          .string()));
}

TEST(TGeoOCCTSolid, BoxContainsUsesCmCoordinates) {
  auto solid                = LoadBox();
  const Double_t inside[3]  = {0.0, 0.0, 0.0};
  const Double_t surface[3] = {1.0, 0.0, 0.0};
  const Double_t outside[3] = {1.2, 0.0, 0.0};
  EXPECT_TRUE(solid->Contains(inside));
  EXPECT_TRUE(solid->Contains(surface));
  EXPECT_FALSE(solid->Contains(outside));
}

TEST(TGeoOCCTSolid, DistancesUseCmUnits) {
  auto solid                = LoadBox();
  const Double_t outside[3] = {2.0, 0.0, 0.0};
  const Double_t inside[3]  = {0.0, 0.0, 0.0};
  const Double_t to_box[3]  = {-1.0, 0.0, 0.0};
  const Double_t out_x[3]   = {1.0, 0.0, 0.0};
  EXPECT_NEAR(solid->DistFromOutside(outside, to_box), 1.0, 1e-9);
  EXPECT_NEAR(solid->DistFromInside(inside, out_x), 1.0, 1e-9);
}

TEST(TGeoOCCTSolid, RayMissReturnsTGeoBig) {
  auto solid                = LoadBox();
  const Double_t outside[3] = {2.0, 0.0, 0.0};
  const Double_t away[3]    = {1.0, 0.0, 0.0};
  EXPECT_EQ(solid->DistFromOutside(outside, away), TGeoShape::Big());
}

TEST(TGeoOCCTSolid, ComputeNormalUsesExactKernel) {
  auto solid              = LoadBox();
  const Double_t point[3] = {1.0, 0.0, 0.0};
  const Double_t dir[3]   = {1.0, 0.0, 0.0};
  Double_t norm[3]        = {0.0, 0.0, 0.0};
  solid->ComputeNormal(point, dir, norm);
  EXPECT_NEAR(norm[0], 1.0, 1e-9);
  EXPECT_NEAR(norm[1], 0.0, 1e-9);
  EXPECT_NEAR(norm[2], 0.0, 1e-9);
}

TEST(TGeoOCCTSolid, BBoxAndCapacityAreInCm) {
  auto solid   = LoadBox();
  Double_t xlo = 0.0;
  Double_t xhi = 0.0;
  Double_t ylo = 0.0;
  Double_t yhi = 0.0;
  Double_t zlo = 0.0;
  Double_t zhi = 0.0;
  EXPECT_NEAR(solid->GetAxisRange(1, xlo, xhi), 2.0, 1e-9);
  EXPECT_NEAR(xlo, -1.0, 1e-9);
  EXPECT_NEAR(xhi, 1.0, 1e-9);
  EXPECT_NEAR(solid->GetAxisRange(2, ylo, yhi), 3.0, 1e-9);
  EXPECT_NEAR(ylo, -1.5, 1e-9);
  EXPECT_NEAR(yhi, 1.5, 1e-9);
  EXPECT_NEAR(solid->GetAxisRange(3, zlo, zhi), 4.0, 1e-9);
  EXPECT_NEAR(zlo, -2.0, 1e-9);
  EXPECT_NEAR(zhi, 2.0, 1e-9);
  EXPECT_NEAR(solid->Capacity(), 24.0, 1e-9);
}

TEST(TGeoOCCTSolid, SphereSafetyUsesCmUnits) {
  auto solid                = LoadSphere();
  const Double_t inside[3]  = {0.0, 0.0, 0.0};
  const Double_t outside[3] = {2.0, 0.0, 0.0};
  EXPECT_GT(solid->Safety(inside, kTRUE), 0.0);
  EXPECT_LE(solid->Safety(inside, kTRUE), 1.5);
  EXPECT_NEAR(solid->Safety(outside, kFALSE), 0.5, 1e-6);
}

} // namespace
