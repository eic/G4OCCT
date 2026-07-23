// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include "TGeoOCCTSolidBridge.hh"

#include "G4OCCTSolidKernel.hh"

#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>

#include <G4ThreeVector.hh>

#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace g4occt::detail {

namespace {

constexpr double kCmToMm = 10.0;
constexpr double kMmToCm = 0.1;
constexpr double kMm3ToCm3 = 0.001;

G4ThreeVector ToKernelPoint(const double* point_cm) {
  return G4ThreeVector(point_cm[0] * kCmToMm, point_cm[1] * kCmToMm, point_cm[2] * kCmToMm);
}

G4ThreeVector ToKernelDirection(const double* dir) {
  return G4ThreeVector(dir[0], dir[1], dir[2]);
}

void WriteNormal(const G4ThreeVector& normal, double* out) {
  out[0] = normal.x();
  out[1] = normal.y();
  out[2] = normal.z();
}

struct ThreadCaches {
  G4OCCTSolidKernel::ClassifierCache classifier;
  G4OCCTSolidKernel::IntersectorCache intersector;
  G4OCCTSolidKernel::SphereCacheData sphere;
};

} // namespace

class TGeoOCCTSolidBridge::Impl {
public:
  explicit Impl(const TopoDS_Shape& shape) : kernel(shape) {}

  ThreadCaches& CachesForThisThread() const {
    static thread_local std::unordered_map<const Impl*, ThreadCaches> caches;
    return caches[this];
  }

  G4OCCTSolidKernel kernel;
  mutable std::optional<DisplayMeshCm> displayMesh;
  mutable std::uint64_t displayMeshGeneration{std::numeric_limits<std::uint64_t>::max()};
  mutable std::mutex displayMeshMutex;
};

double TGeoOCCTSolidBridge::InfinityCm() { return G4OCCTSolidKernel::Infinity() * kMmToCm; }

TGeoOCCTSolidBridge::TGeoOCCTSolidBridge(const TopoDS_Shape& shape)
    : fImpl(std::make_unique<Impl>(shape)) {}

TGeoOCCTSolidBridge::~TGeoOCCTSolidBridge() = default;

std::unique_ptr<TGeoOCCTSolidBridge>
TGeoOCCTSolidBridge::FromSTEP(const std::string& name, const std::string& path) {
  STEPControl_Reader reader;
  const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
  if (status != IFSelect_RetDone) {
    throw std::runtime_error("TGeoOCCTSolidBridge::FromSTEP: failed to read STEP file \"" + path +
                             "\"");
  }
  if (reader.NbRootsForTransfer() == 0) {
    throw std::runtime_error("TGeoOCCTSolidBridge::FromSTEP: STEP file \"" + path +
                             "\" contains no root shapes");
  }
  reader.TransferRoots();
  const TopoDS_Shape shape = reader.OneShape();
  if (shape.IsNull()) {
    throw std::runtime_error("TGeoOCCTSolidBridge::FromSTEP: STEP file \"" + path +
                             "\" yielded a null shape for \"" + name + "\"");
  }
  return std::make_unique<TGeoOCCTSolidBridge>(shape);
}

const TopoDS_Shape& TGeoOCCTSolidBridge::Shape() const { return fImpl->kernel.Shape(); }

void TGeoOCCTSolidBridge::SetShape(const TopoDS_Shape& shape) { fImpl->kernel.SetShape(shape); }

TGeoOCCTSolidBridge::BoundsCm TGeoOCCTSolidBridge::Bounds() const {
  const auto& bounds = fImpl->kernel.Bounds();
  const G4ThreeVector center_mm = 0.5 * (bounds.min + bounds.max);
  const G4ThreeVector half_mm = 0.5 * (bounds.max - bounds.min);
  return BoundsCm{
      .dx = half_mm.x() * kMmToCm,
      .dy = half_mm.y() * kMmToCm,
      .dz = half_mm.z() * kMmToCm,
      .origin = {center_mm.x() * kMmToCm, center_mm.y() * kMmToCm, center_mm.z() * kMmToCm},
  };
}

const TGeoOCCTSolidBridge::DisplayMeshCm& TGeoOCCTSolidBridge::DisplayMesh() const {
  std::unique_lock<std::mutex> lock(fImpl->displayMeshMutex);
  const std::uint64_t generation = fImpl->kernel.ShapeGeneration();
  if (fImpl->displayMesh.has_value() && fImpl->displayMeshGeneration == generation) {
    return *fImpl->displayMesh;
  }

  DisplayMeshCm mesh;
  const auto& surfaceCache = fImpl->kernel.GetOrBuildSurfaceCache();
  mesh.triangles.reserve(surfaceCache.triangles.size());
  for (const auto& tri : surfaceCache.triangles) {
    mesh.triangles.push_back(DisplayTriangleCm{
        .p1 = {tri.p1.x() * kMmToCm, tri.p1.y() * kMmToCm, tri.p1.z() * kMmToCm},
        .p2 = {tri.p2.x() * kMmToCm, tri.p2.y() * kMmToCm, tri.p2.z() * kMmToCm},
        .p3 = {tri.p3.x() * kMmToCm, tri.p3.y() * kMmToCm, tri.p3.z() * kMmToCm},
    });
  }
  fImpl->displayMesh = std::move(mesh);
  fImpl->displayMeshGeneration = generation;
  return *fImpl->displayMesh;
}

std::uint64_t TGeoOCCTSolidBridge::ShapeGeneration() const { return fImpl->kernel.ShapeGeneration(); }

double TGeoOCCTSolidBridge::CapacityCm3() const {
  return fImpl->kernel.GetCubicVolume() * kMm3ToCm3;
}

double TGeoOCCTSolidBridge::SafetyCm(const double* point_cm, bool inside) const {
  ThreadCaches& caches = fImpl->CachesForThisThread();
  const G4ThreeVector point_mm = ToKernelPoint(point_cm);
  const double safety_mm = inside ? fImpl->kernel.DistanceToOut(point_mm, caches.sphere)
                                  : fImpl->kernel.DistanceToIn(point_mm, caches.classifier);
  return safety_mm * kMmToCm;
}

double TGeoOCCTSolidBridge::DistFromOutsideCm(const double* point_cm, const double* dir,
                                              double* safe_cm) const {
  ThreadCaches& caches = fImpl->CachesForThisThread();
  const G4ThreeVector point_mm = ToKernelPoint(point_cm);
  if (safe_cm != nullptr) {
    *safe_cm = fImpl->kernel.DistanceToIn(point_mm, caches.classifier) * kMmToCm;
  }
  const double dist_mm =
      fImpl->kernel.DistanceToIn(point_mm, ToKernelDirection(dir), caches.intersector);
  return dist_mm * kMmToCm;
}

double TGeoOCCTSolidBridge::DistFromInsideCm(const double* point_cm, const double* dir,
                                             double* safe_cm, bool calcNorm, double* norm,
                                             bool* validNorm) const {
  ThreadCaches& caches = fImpl->CachesForThisThread();
  const G4ThreeVector point_mm = ToKernelPoint(point_cm);
  if (safe_cm != nullptr) {
    *safe_cm = fImpl->kernel.DistanceToOut(point_mm, caches.sphere) * kMmToCm;
  }

  G4bool g4ValidNorm = false;
  G4ThreeVector g4Norm;
  const double dist_mm = fImpl->kernel.DistanceToOut(
      point_mm, ToKernelDirection(dir), caches.intersector, calcNorm, &g4ValidNorm,
      calcNorm ? &g4Norm : nullptr);
  if (validNorm != nullptr) {
    *validNorm = g4ValidNorm;
  }
  if (calcNorm && norm != nullptr && g4ValidNorm) {
    WriteNormal(g4Norm, norm);
  }
  return dist_mm * kMmToCm;
}

TGeoOCCTSolidBridge::PointClassification
TGeoOCCTSolidBridge::ClassifyCm(const double* point_cm) const {
  ThreadCaches& caches = fImpl->CachesForThisThread();
  return static_cast<PointClassification>(
      fImpl->kernel.ClassifyPoint(ToKernelPoint(point_cm), caches.classifier, caches.intersector,
                                  caches.sphere));
}

void TGeoOCCTSolidBridge::SurfaceNormalCm(const double* point_cm, double* norm) const {
  WriteNormal(fImpl->kernel.SurfaceNormal(ToKernelPoint(point_cm)), norm);
}

} // namespace g4occt::detail
