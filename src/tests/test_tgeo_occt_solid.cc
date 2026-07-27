// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include "G4OCCT/TGeoOCCTSolid.hh"

#include <TBuffer3D.h>
#include <TCanvas.h>
#include <TGeoManager.h>
#include <TGeoMaterial.h>
#include <TGeoMedium.h>
#include <TGeoVolume.h>
#include <TROOT.h>
#include <TGeoShape.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <vector>

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

TEST(TGeoOCCTSolid, DistancesRespectStepCap) {
  auto solid                = LoadBox();
  const Double_t outside[3] = {2.0, 0.0, 0.0};
  const Double_t inside[3]  = {0.0, 0.0, 0.0};
  const Double_t to_box[3]  = {-1.0, 0.0, 0.0};
  const Double_t away[3]    = {1.0, 0.0, 0.0};

  EXPECT_NEAR(solid->DistFromOutside(outside, to_box, 1, 0.25), 0.25, 1e-12);
  EXPECT_NEAR(solid->DistFromInside(inside, away, 1, 0.25), 0.25, 1e-12);
  EXPECT_NEAR(solid->DistFromOutside(outside, away, 1, 0.25), 0.25, 1e-12);
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
  EXPECT_NEAR(solid->GetDX(), 1.0, 1e-9);
  EXPECT_NEAR(solid->GetDY(), 1.5, 1e-9);
  EXPECT_NEAR(solid->GetDZ(), 2.0, 1e-9);
  const auto* origin = solid->GetOrigin();
  ASSERT_NE(origin, nullptr);
  EXPECT_NEAR(origin[0], 0.0, 1e-9);
  EXPECT_NEAR(origin[1], 0.0, 1e-9);
  EXPECT_NEAR(origin[2], 0.0, 1e-9);
  EXPECT_NEAR(solid->Capacity(), 24.0, 1e-9);
}

TEST(TGeoOCCTSolid, SetOCCTShapeRefreshesBBox) {
  auto solid  = LoadBox();
  auto sphere = LoadSphere();

  solid->SetOCCTShape(sphere->GetOCCTShape());

  Double_t xlo = 0.0;
  Double_t xhi = 0.0;
  Double_t ylo = 0.0;
  Double_t yhi = 0.0;
  Double_t zlo = 0.0;
  Double_t zhi = 0.0;
  EXPECT_NEAR(solid->GetAxisRange(1, xlo, xhi), 3.0, 1e-9);
  EXPECT_NEAR(solid->GetAxisRange(2, ylo, yhi), 3.0, 1e-9);
  EXPECT_NEAR(solid->GetAxisRange(3, zlo, zhi), 3.0, 1e-9);
  EXPECT_NEAR(solid->GetDX(), 1.5, 1e-9);
  EXPECT_NEAR(solid->GetDY(), 1.5, 1e-9);
  EXPECT_NEAR(solid->GetDZ(), 1.5, 1e-9);
}

TEST(TGeoOCCTSolid, MeshHooksProduceDrawableMesh) {
  auto solid  = LoadBox();
  Int_t nvert = 0;
  Int_t nsegs = 0;
  Int_t npols = 0;
  solid->GetMeshNumbers(nvert, nsegs, npols);
  EXPECT_GT(nvert, 0);
  EXPECT_GT(nsegs, 0);
  EXPECT_GT(npols, 0);

  std::vector<Double_t> points(static_cast<std::size_t>(3 * nvert), 0.0);
  solid->SetPoints(points.data());
  bool any_nonzero = false;
  for (double v : points) {
    if (v != 0.0) {
      any_nonzero = true;
      break;
    }
  }
  EXPECT_TRUE(any_nonzero);

  std::unique_ptr<TBuffer3D> buffer(solid->MakeBuffer3D());
  ASSERT_NE(buffer, nullptr);
  solid->SetSegsAndPols(*buffer);
}

TEST(TGeoOCCTSolid, SphereSafetyUsesCmUnits) {
  auto solid                = LoadSphere();
  const Double_t inside[3]  = {0.0, 0.0, 0.0};
  const Double_t outside[3] = {2.0, 0.0, 0.0};
  EXPECT_GT(solid->Safety(inside, kTRUE), 0.0);
  EXPECT_LE(solid->Safety(inside, kTRUE), 1.5);
  EXPECT_NEAR(solid->Safety(outside, kFALSE), 0.5, 1e-6);
}

TEST(TGeoOCCTSolid, RootDrawingSmoke) {
  struct RootBatchGuard {
    const Bool_t previousBatchMode;
    ~RootBatchGuard() { gROOT->SetBatch(previousBatchMode); }
  };

  // Delete any TGeoManager that ROOT may have created automatically (e.g. when
  // a previous test in the same process constructed a TGeoShape with
  // gGeoManager == nullptr, causing ROOT to create a "default geometry").
  // We must do this BEFORE creating shapes below; otherwise ROOT's "default
  // geometry" would take ownership of the solid returned by LoadBox(), then
  // delete it when our named TGeoManager is constructed — leaving a dangling
  // pointer.  delete nullptr is a no-op, so this is always safe.
  delete gGeoManager;
  gGeoManager = nullptr;

  const RootBatchGuard batchGuard{gROOT->IsBatch()};
  gROOT->SetBatch(kTRUE);
  {
    // Create the TGeoManager FIRST so that the solid returned by LoadBox()
    // below does not cause ROOT to auto-create a "default geometry" manager.
    // If LoadBox() were called while gGeoManager == nullptr, ROOT would create
    // a "default geometry" that takes ownership of the solid; constructing the
    // named manager below would then delete the default geometry — and the solid
    // with it — leaving a dangling pointer.
    //
    // Root ownership differs by shape construction path. Register the shape
    // exactly once: add it only when it is not already present in the manager's
    // shape list, so teardown avoids both leaks and duplicate-delete hazards.
    TGeoManager manager("geom", "geom");
    auto* material = new TGeoMaterial("mat", 1.0, 1.0, 1.0);
    auto* medium   = new TGeoMedium("med", 1, material);
    auto* top      = manager.MakeBox("world", medium, 100.0, 100.0, 100.0);
    manager.SetTopVolume(top);
    auto* innerShape = LoadBox().release();
    if (!manager.GetListOfShapes()->FindObject(innerShape)) {
      manager.AddShape(innerShape);
    }
    auto* inner = new TGeoVolume("inner", innerShape, medium);
    top->AddNode(inner, 1);
    manager.CloseGeometry();

    TCanvas canvas("tgeoocctsolid", "tgeoocctsolid", 400, 300);
    inner->Draw();
    canvas.Update();
  }
  // The stack TGeoManager was destructed at end of the block above; ROOT sets
  // gGeoManager = nullptr in TGeoManager::~TGeoManager when it is the active
  // manager.  Null it explicitly for robustness.
  gGeoManager = nullptr;
}

} // namespace
