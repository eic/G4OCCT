// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file TGeoOCCTSolid.cc
/// @brief ROOT/TGeo adapter over the shared G4OCCT solid-query kernel.

#include "G4OCCT/TGeoOCCTSolid.hh"

#include "TGeoOCCTSolidBridge.hh"

#include <TBuffer3D.h>
#include <TError.h>
#include <TGeoBBox.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

TGeoOCCTSolid::TGeoOCCTSolid() = default;

TGeoOCCTSolid::TGeoOCCTSolid(const char* name, const TopoDS_Shape& shape)
    : TGeoShape(name), fBridge(std::make_unique<g4occt::detail::TGeoOCCTSolidBridge>(shape)) {
  ComputeBBox();
}

TGeoOCCTSolid::~TGeoOCCTSolid() = default;

TGeoOCCTSolid* TGeoOCCTSolid::FromSTEP(const char* name, const std::string& path) {
  auto result = std::unique_ptr<TGeoOCCTSolid>(new TGeoOCCTSolid());
  result->SetName(name);
  result->fBridge = g4occt::detail::TGeoOCCTSolidBridge::FromSTEP(name, path);
  result->ComputeBBox();
  return result.release();
}

Double_t TGeoOCCTSolid::Capacity() const {
  return fBridge ? fBridge->CapacityCm3() : 0.0;
}

void TGeoOCCTSolid::ComputeBBox() {
  EnsureBBoxHelper();
  const auto bounds = fBridge->Bounds();
  Double_t origin[3] = {bounds.origin[0], bounds.origin[1], bounds.origin[2]};
  fBBoxHelper = std::make_unique<TGeoBBox>(GetName(), bounds.dx, bounds.dy, bounds.dz, origin);
}

void TGeoOCCTSolid::ComputeNormal(const Double_t* point, const Double_t* /*dir*/,
                                  Double_t* norm) const {
  if (!fBridge) {
    norm[0] = 0.0;
    norm[1] = 0.0;
    norm[2] = 1.0;
    return;
  }
  fBridge->SurfaceNormalCm(point, norm);
}

Bool_t TGeoOCCTSolid::Contains(const Double_t* point) const {
  if (!fBridge) {
    return kFALSE;
  }
  const auto classification = fBridge->ClassifyCm(point);
  return classification != g4occt::detail::TGeoOCCTSolidBridge::PointClassification::kOutside;
}

Bool_t TGeoOCCTSolid::CouldBeCrossed(const Double_t* point, const Double_t* dir) const {
  EnsureBBoxHelper();
  return fBBoxHelper->CouldBeCrossed(point, dir);
}

Int_t TGeoOCCTSolid::DistancetoPrimitive(Int_t px, Int_t py) {
  EnsureBBoxHelper();
  return fBBoxHelper->DistancetoPrimitive(px, py);
}

Double_t TGeoOCCTSolid::DistFromInside(const Double_t* point, const Double_t* dir, Int_t /*iact*/,
                                       Double_t /*step*/, Double_t* safe) const {
  if (!fBridge) {
    return TGeoShape::Big();
  }
  const double dist = fBridge->DistFromInsideCm(point, dir, safe);
  return (std::isinf(dist) || dist >= 0.5 * g4occt::detail::TGeoOCCTSolidBridge::InfinityCm())
             ? TGeoShape::Big()
             : std::min(dist, TGeoShape::Big());
}

Double_t TGeoOCCTSolid::DistFromOutside(const Double_t* point, const Double_t* dir, Int_t /*iact*/,
                                        Double_t /*step*/, Double_t* safe) const {
  if (!fBridge) {
    return TGeoShape::Big();
  }
  const double dist = fBridge->DistFromOutsideCm(point, dir, safe);
  return (std::isinf(dist) || dist >= 0.5 * g4occt::detail::TGeoOCCTSolidBridge::InfinityCm())
             ? TGeoShape::Big()
             : std::min(dist, TGeoShape::Big());
}

TGeoVolume* TGeoOCCTSolid::Divide(TGeoVolume* /*voldiv*/, const char* /*divname*/, Int_t /*iaxis*/,
                                  Int_t /*ndiv*/, Double_t /*start*/, Double_t /*step*/) {
  Error("Divide", "TGeoOCCTSolid does not support ROOT volume divisions");
  return nullptr;
}

const char* TGeoOCCTSolid::GetAxisName(Int_t iaxis) const {
  EnsureBBoxHelper();
  return fBBoxHelper->GetAxisName(iaxis);
}

Double_t TGeoOCCTSolid::GetAxisRange(Int_t iaxis, Double_t& xlo, Double_t& xhi) const {
  EnsureBBoxHelper();
  return fBBoxHelper->GetAxisRange(iaxis, xlo, xhi);
}

void TGeoOCCTSolid::GetBoundingCylinder(Double_t* param) const {
  EnsureBBoxHelper();
  fBBoxHelper->GetBoundingCylinder(param);
}

Int_t TGeoOCCTSolid::GetByteCount() const { return 6 * static_cast<Int_t>(sizeof(Double_t)); }

Bool_t TGeoOCCTSolid::GetPointsOnSegments(Int_t npoints, Double_t* array) const {
  EnsureBBoxHelper();
  return fBBoxHelper->GetPointsOnSegments(npoints, array);
}

Int_t TGeoOCCTSolid::GetFittingBox(const TGeoBBox* parambox, TGeoMatrix* mat, Double_t& dx,
                                   Double_t& dy, Double_t& dz) const {
  EnsureBBoxHelper();
  return fBBoxHelper->GetFittingBox(parambox, mat, dx, dy, dz);
}

TGeoShape* TGeoOCCTSolid::GetMakeRuntimeShape(TGeoShape* /*mother*/, TGeoMatrix* /*mat*/) const {
  if (!fBridge) {
    return nullptr;
  }
  auto* runtime = new TGeoOCCTSolid(GetName(), GetOCCTShape());
  runtime->SetRuntime();
  return runtime;
}

Bool_t TGeoOCCTSolid::IsCylType() const { return kFALSE; }

Bool_t TGeoOCCTSolid::IsValidBox() const {
  EnsureBBoxHelper();
  return fBBoxHelper->IsValidBox();
}

void TGeoOCCTSolid::InspectShape() const {
  if (!fBridge) {
    Info("InspectShape", "Uninitialized TGeoOCCTSolid");
    return;
  }
  const auto bounds = fBridge->Bounds();
  Info("InspectShape",
       "TGeoOCCTSolid '%s' bbox=(dx=%g, dy=%g, dz=%g) cm origin=(%g,%g,%g) cm",
       GetName(), bounds.dx, bounds.dy, bounds.dz, bounds.origin[0], bounds.origin[1],
       bounds.origin[2]);
}

Double_t TGeoOCCTSolid::Safety(const Double_t* point, Bool_t in) const {
  return fBridge ? fBridge->SafetyCm(point, in == kTRUE) : 0.0;
}

void TGeoOCCTSolid::SetDimensions(Double_t* /*param*/) {
  Error("SetDimensions",
        "TGeoOCCTSolid does not support parameterized dimension mutation; use SetOCCTShape");
  if (fBridge) {
    ComputeBBox();
  }
}

void TGeoOCCTSolid::SetPoints(Double_t* points) const {
  EnsureBBoxHelper();
  fBBoxHelper->SetPoints(points);
}

void TGeoOCCTSolid::SetPoints(Float_t* points) const {
  EnsureBBoxHelper();
  fBBoxHelper->SetPoints(points);
}

void TGeoOCCTSolid::SetSegsAndPols(TBuffer3D& buff) const {
  EnsureBBoxHelper();
  fBBoxHelper->SetSegsAndPols(buff);
}

void TGeoOCCTSolid::Sizeof3D() const {
  EnsureBBoxHelper();
  fBBoxHelper->Sizeof3D();
}

const TopoDS_Shape& TGeoOCCTSolid::GetOCCTShape() const { return fBridge->Shape(); }

void TGeoOCCTSolid::SetOCCTShape(const TopoDS_Shape& shape) {
  if (!fBridge) {
    fBridge = std::make_unique<g4occt::detail::TGeoOCCTSolidBridge>(shape);
  } else {
    fBridge->SetShape(shape);
  }
  ComputeBBox();
}

void TGeoOCCTSolid::EnsureBBoxHelper() const {
  if (!fBBoxHelper) {
    if (!fBridge) {
      Double_t origin[3] = {0.0, 0.0, 0.0};
      fBBoxHelper = std::make_unique<TGeoBBox>(GetName(), 0.0, 0.0, 0.0, origin);
    } else {
      const auto bounds = fBridge->Bounds();
      Double_t origin[3] = {bounds.origin[0], bounds.origin[1], bounds.origin[2]};
      fBBoxHelper = std::make_unique<TGeoBBox>(GetName(), bounds.dx, bounds.dy, bounds.dz,
                                               origin);
    }
  }
}
