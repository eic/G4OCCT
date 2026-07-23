// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file G4OCCTSolidKernel.hh
/// @brief Shared OCCT-backed solid query kernel for adapter frontends.

#ifndef G4OCCT_src_G4OCCTSolidKernel_hh
#define G4OCCT_src_G4OCCTSolidKernel_hh

#include <G4ThreeVector.hh>
#include <geomdefs.hh>

#include <BRepAdaptor_Surface.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_TriangleSet.hxx>
#include <Bnd_Box.hxx>
#include <IntCurvesFace_Intersector.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_TShape.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt2d.hxx>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace g4occt::detail {

class G4OCCTSolidKernel {
public:
  static G4double Infinity() { return ::kInfinity; }

  enum class PointClassification { kInside, kSurface, kOutside };

  struct AxisAlignedBounds {
    G4ThreeVector min;
    G4ThreeVector max;
  };

  struct ClassifierCache {
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
    std::optional<BRepClass3d_SolidClassifier> classifier;
  };

  struct IntersectorCache {
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
    std::vector<std::unique_ptr<IntCurvesFace_Intersector>> faceIntersectors;
    std::vector<Bnd_Box> expandedBoxes;
  };

  struct InscribedSphere {
    G4ThreeVector centre;
    G4double radius;
  };

  struct SphereCacheData {
    std::vector<InscribedSphere> spheres;
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
  };

  static constexpr std::size_t kMaxInscribedSpheres = 64;

  struct SurfaceTriangle {
    G4ThreeVector p1, p2, p3;
    std::uint32_t faceIndex;
  };

  struct SurfaceSamplingCache {
    std::vector<TopoDS_Face> faces;
    std::vector<SurfaceTriangle> triangles;
    std::vector<G4double> cumulativeAreas;
    G4double totalArea{0.0};
  };

  explicit G4OCCTSolidKernel(const TopoDS_Shape& shape);

  void SetShape(const TopoDS_Shape& shape);

  const TopoDS_Shape& Shape() const { return fShape; }
  std::uint64_t ShapeGeneration() const { return fShapeGeneration.load(std::memory_order_acquire); }
  const AxisAlignedBounds& Bounds() const { return fCachedBounds; }

  PointClassification ClassifyPoint(const G4ThreeVector& p, ClassifierCache& classifierCache,
                                    IntersectorCache& intersectorCache,
                                    SphereCacheData& sphereCache) const;
  G4ThreeVector SurfaceNormal(const G4ThreeVector& p) const;
  G4double DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v,
                        IntersectorCache& intersectorCache) const;
  G4double DistanceToIn(const G4ThreeVector& p, ClassifierCache& classifierCache) const;
  G4double DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                         IntersectorCache& intersectorCache, const G4bool calcNorm = false,
                         G4bool* validNorm = nullptr, G4ThreeVector* n = nullptr) const;
  G4double DistanceToOut(const G4ThreeVector& p, SphereCacheData& sphereCache) const;
  G4double ExactDistanceToIn(const G4ThreeVector& p, ClassifierCache& classifierCache) const;
  G4double ExactDistanceToOut(const G4ThreeVector& p) const;
  G4double GetCubicVolume();
  G4double GetSurfaceArea();
  G4ThreeVector GetPointOnSurface() const;
  const SurfaceSamplingCache& GetOrBuildSurfaceCache() const;

private:
  struct FaceBounds {
    TopoDS_Face face;
    Bnd_Box box;
    BRepAdaptor_Surface adaptor;
    std::optional<gp_Pln> plane;
    std::vector<gp_Pnt2d> uvPolygon;
    std::optional<G4ThreeVector> outwardNormal;
  };

  struct ClosestFaceMatch {
    TopoDS_Face face;
    G4double distance{Infinity()};
    std::size_t faceIndex{0};
    std::optional<std::pair<Standard_Real, Standard_Real>> uv;
  };

  BRepClass3d_SolidClassifier& GetOrCreateClassifier(ClassifierCache& cache) const;
  IntersectorCache& GetOrCreateIntersector(IntersectorCache& cache) const;
  SphereCacheData& GetOrInitSphereCache(SphereCacheData& cache) const;
  void TryInsertSphere(SphereCacheData& cache, const G4ThreeVector& centre, G4double d) const;
  void ComputeBounds();
  void ComputeInitialSpheres();
  G4double AABBLowerBound(const G4ThreeVector& p) const;
  G4double BVHLowerBoundDistance(const G4ThreeVector& p) const;
  G4double PlanarFaceLowerBoundDistance(const G4ThreeVector& p) const;
  static std::optional<ClosestFaceMatch>
  TryFindClosestFace(const std::vector<FaceBounds>& faceBoundsCache, const G4ThreeVector& point,
                     G4double maxDistance = Infinity());

  TopoDS_Shape fShape;
  AxisAlignedBounds fCachedBounds;
  std::vector<FaceBounds> fFaceBoundsCache;
  std::unordered_map<const TopoDS_TShape*, std::vector<std::size_t>> fFaceAdaptorIndex;
  std::vector<InscribedSphere> fInitialSpheres;
  Handle(BRepExtrema_TriangleSet) fTriangleSet;
  G4double fBVHDeflection{0.0};
  std::vector<G4double> fFaceDeflections;
  bool fAllFacesPlanar{false};
  std::atomic<std::uint64_t> fShapeGeneration{0};
  mutable std::optional<G4double> fCachedVolume;
  mutable std::optional<G4double> fCachedSurfaceArea;
  mutable std::mutex fVolumeAreaMutex;
  mutable std::optional<SurfaceSamplingCache> fSurfaceCache;
  mutable std::uint64_t fSurfaceCacheGeneration{std::numeric_limits<std::uint64_t>::max()};
  mutable std::mutex fSurfaceCacheMutex;
};

} // namespace g4occt::detail

#endif // G4OCCT_src_G4OCCTSolidKernel_hh
