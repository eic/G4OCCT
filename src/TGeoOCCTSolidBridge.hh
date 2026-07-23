// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file TGeoOCCTSolidBridge.hh
/// @brief ROOT-independent bridge from TGeoOCCTSolid to the shared OCCT kernel.

#ifndef G4OCCT_src_TGeoOCCTSolidBridge_hh
#define G4OCCT_src_TGeoOCCTSolidBridge_hh

#include <array>
#include <memory>
#include <string>

class TopoDS_Shape;

namespace g4occt::detail {

class TGeoOCCTSolidBridge {
public:
  enum class PointClassification { kInside, kSurface, kOutside };

  static double InfinityCm();

  struct BoundsCm {
    double dx{0.0};
    double dy{0.0};
    double dz{0.0};
    std::array<double, 3> origin{0.0, 0.0, 0.0};
  };

  explicit TGeoOCCTSolidBridge(const TopoDS_Shape& shape);
  ~TGeoOCCTSolidBridge();

  static std::unique_ptr<TGeoOCCTSolidBridge> FromSTEP(const std::string& name,
                                                       const std::string& path);

  TGeoOCCTSolidBridge(const TGeoOCCTSolidBridge&)            = delete;
  TGeoOCCTSolidBridge& operator=(const TGeoOCCTSolidBridge&) = delete;

  const TopoDS_Shape& Shape() const;
  void SetShape(const TopoDS_Shape& shape);

  BoundsCm Bounds() const;
  double CapacityCm3() const;
  double SafetyCm(const double* point_cm, bool inside) const;
  double DistFromOutsideCm(const double* point_cm, const double* dir,
                           double* safe_cm = nullptr) const;
  double DistFromInsideCm(const double* point_cm, const double* dir, double* safe_cm = nullptr,
                          bool calcNorm = false, double* norm = nullptr,
                          bool* validNorm = nullptr) const;
  PointClassification ClassifyCm(const double* point_cm) const;
  void SurfaceNormalCm(const double* point_cm, double* norm) const;

private:
  class Impl;
  std::unique_ptr<Impl> fImpl;
};

} // namespace g4occt::detail

#endif // G4OCCT_src_TGeoOCCTSolidBridge_hh
