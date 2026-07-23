// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file TGeoOCCTSolid.hh
/// @brief ROOT/TGeo shape adapter over the shared G4OCCT solid-query kernel.

#ifndef G4OCCT_TGeoOCCTSolid_hh
#define G4OCCT_TGeoOCCTSolid_hh

#include <TGeoShape.h>

#include <memory>
#include <string>

class TGeoBBox;
class TopoDS_Shape;
class TGeoMatrix;
class TBuffer3D;
class TGeoVolume;

namespace g4occt::detail {
class TGeoOCCTSolidBridge;
}

class TGeoOCCTSolid : public TGeoShape {
public:
  TGeoOCCTSolid();
  TGeoOCCTSolid(const char* name, const TopoDS_Shape& shape);
  ~TGeoOCCTSolid() override;

  TGeoOCCTSolid(const TGeoOCCTSolid&) = delete;
  TGeoOCCTSolid& operator=(const TGeoOCCTSolid&) = delete;

  static TGeoOCCTSolid* FromSTEP(const char* name, const std::string& path);

  Double_t Capacity() const override;
  void ComputeBBox() override;
  void ComputeNormal(const Double_t* point, const Double_t* dir, Double_t* norm) const override;
  Bool_t Contains(const Double_t* point) const override;
  Bool_t CouldBeCrossed(const Double_t* point, const Double_t* dir) const override;
  Int_t DistancetoPrimitive(Int_t px, Int_t py) override;
  Double_t DistFromInside(const Double_t* point, const Double_t* dir, Int_t iact = 1,
                          Double_t step = TGeoShape::Big(),
                          Double_t* safe = nullptr) const override;
  Double_t DistFromOutside(const Double_t* point, const Double_t* dir, Int_t iact = 1,
                           Double_t step = TGeoShape::Big(),
                           Double_t* safe = nullptr) const override;
  TGeoVolume* Divide(TGeoVolume* voldiv, const char* divname, Int_t iaxis, Int_t ndiv,
                     Double_t start, Double_t step) override;
  const char* GetAxisName(Int_t iaxis) const override;
  Double_t GetAxisRange(Int_t iaxis, Double_t& xlo, Double_t& xhi) const override;
  void GetBoundingCylinder(Double_t* param) const override;
  Int_t GetByteCount() const override;
  Bool_t GetPointsOnSegments(Int_t npoints, Double_t* array) const override;
  Int_t GetFittingBox(const TGeoBBox* parambox, TGeoMatrix* mat, Double_t& dx, Double_t& dy,
                      Double_t& dz) const override;
  TGeoShape* GetMakeRuntimeShape(TGeoShape* mother, TGeoMatrix* mat) const override;
  Bool_t IsCylType() const override;
  Bool_t IsValidBox() const override;
  void InspectShape() const override;
  Double_t Safety(const Double_t* point, Bool_t in = kTRUE) const override;
  void SetDimensions(Double_t* param) override;
  void SetPoints(Double_t* points) const override;
  void SetPoints(Float_t* points) const override;
  void SetSegsAndPols(TBuffer3D& buff) const override;
  void Sizeof3D() const override;

  const TopoDS_Shape& GetOCCTShape() const;
  void SetOCCTShape(const TopoDS_Shape& shape);

private:
  void EnsureBBoxHelper() const;

  std::unique_ptr<g4occt::detail::TGeoOCCTSolidBridge> fBridge;
  mutable std::unique_ptr<TGeoBBox> fBBoxHelper;
};

#endif // G4OCCT_TGeoOCCTSolid_hh
