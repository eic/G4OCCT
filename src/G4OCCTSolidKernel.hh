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
#include <gp_Pln.hxx>
#include <gp_Pnt2d.hxx>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace g4occt::detail {

/**
 * Relative tessellation deflection shared by kernel-side BVH/surface-sampling
 * meshing and adapter-side visualisation meshing.
 */
inline constexpr Standard_Real kOCCTRelativeDeflection = 0.01;

/**
 * @brief Shared OCCT-backed query kernel used by adapter frontends.
 *
 * `G4OCCTSolidKernel` owns the shape-dependent state that is common to all
 * frontends over the OCCT solid-query implementation: cached bounds, per-face
 * metadata, tessellation/BVH state, and generation-tagged caches for exact
 * OCCT query helpers.
 *
 * The kernel is intentionally Geant4-dependent today because it reuses
 * `G4ThreeVector`, `G4double`, `kInfinity`, and the Geant4 tolerance/error
 * model.  It is nevertheless separated from `G4OCCTSolid` so that adapter
 * responsibilities such as `G4VSolid` integration, worker-thread `G4Cache`
 * ownership, and visualisation-specific polyhedron generation remain outside
 * the core shape-query implementation.
 */
class G4OCCTSolidKernel {
public:
  /// Return the Geant4 navigation infinity sentinel used by the kernel.
  static G4double Infinity() { return ::kInfinity; }

  /// Classification result for point-in-solid queries.
  enum class PointClassification { kInside, kSurface, kOutside };

  /// Axis-aligned bounds in the imported OCCT coordinate system.
  struct AxisAlignedBounds {
    G4ThreeVector min;
    G4ThreeVector max;
  };

  /**
   * Per-thread cache wrapper for `BRepClass3d_SolidClassifier`.
   *
   * The generation tracks which shape revision the classifier was built for so
   * callers can lazily rebuild the cache after `SetShape()`.
   */
  struct ClassifierCache {
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
    std::optional<BRepClass3d_SolidClassifier> classifier;
  };

  /**
   * Per-thread cache wrapper for per-face ray intersectors and expanded bounds.
   *
   * The intersector vector mirrors `fFaceBoundsCache` entry-for-entry.
   */
  struct IntersectorCache {
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
    std::vector<std::unique_ptr<IntCurvesFace_Intersector>> faceIntersectors;
    std::vector<Bnd_Box> expandedBoxes;
  };

  /// Cached inscribed sphere used to accelerate deep-interior classifications.
  struct InscribedSphere {
    G4ThreeVector centre;
    G4double radius;
  };

  /**
   * Per-thread cache of inscribed spheres for `Inside()` and `DistanceToOut()`.
   */
  struct SphereCacheData {
    std::vector<InscribedSphere> spheres;
    std::uint64_t generation{std::numeric_limits<std::uint64_t>::max()};
  };

  /// Maximum number of inscribed spheres retained in a per-thread sphere cache.
  static constexpr std::size_t kMaxInscribedSpheres = 64;

  /// Tessellated triangle entry used for random surface-point sampling.
  struct SurfaceTriangle {
    G4ThreeVector p1, p2, p3;
    std::uint32_t faceIndex;
  };

  /**
   * Shared cache for `GetPointOnSurface()`.
   *
   * `faces` and `triangles` are parallel through `SurfaceTriangle::faceIndex`;
   * `cumulativeAreas` stores prefix sums for area-weighted random selection.
   */
  struct SurfaceSamplingCache {
    std::vector<TopoDS_Face> faces;
    std::vector<SurfaceTriangle> triangles;
    std::vector<G4double> cumulativeAreas;
    G4double totalArea{0.0};
  };

  /**
   * Construct the kernel around an OCCT shape and eagerly derive
   * shape-dependent metadata.
   *
   * @throws std::invalid_argument if @p shape is null or has no computable
   * bounding box.
   */
  explicit G4OCCTSolidKernel(const TopoDS_Shape& shape);

  /**
   * Replace the current shape and invalidate all generation-tagged caches.
   *
   * Existing per-thread cache objects become stale and will rebuild lazily on
   * their next use.
   *
   * @throws std::invalid_argument if @p shape is null or has no computable
   * bounding box.
   */
  void SetShape(const TopoDS_Shape& shape);

  /// Read-only access to the currently wrapped OCCT shape.
  const TopoDS_Shape& Shape() const { return fShape; }

  /// Monotonic generation counter incremented after successful `SetShape()`.
  std::uint64_t ShapeGeneration() const { return fShapeGeneration.load(std::memory_order_acquire); }

  /// Return the cached axis-aligned bounds for the current shape.
  const AxisAlignedBounds& Bounds() const { return fCachedBounds; }

  /// Classify a point as inside, on, or outside the solid.
  PointClassification ClassifyPoint(const G4ThreeVector& p, ClassifierCache& classifierCache,
                                    IntersectorCache& intersectorCache,
                                    SphereCacheData& sphereCache) const;

  /// Return the outward surface normal at the face nearest point @p p.
  G4ThreeVector SurfaceNormal(const G4ThreeVector& p) const;

  /// Exact ray distance from an exterior point to the first entry intersection.
  G4double DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v,
                        IntersectorCache& intersectorCache) const;

  /// Conservative lower bound on the shortest exterior distance to the solid.
  G4double DistanceToIn(const G4ThreeVector& p, ClassifierCache& classifierCache) const;

  /// Exact ray distance from an interior point to the first exit intersection.
  G4double DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                         IntersectorCache& intersectorCache, const G4bool calcNorm = false,
                         G4bool* validNorm = nullptr, G4ThreeVector* n = nullptr) const;

  /// Conservative lower bound on the shortest interior distance to the surface.
  G4double DistanceToOut(const G4ThreeVector& p, SphereCacheData& sphereCache) const;

  /// Exact shortest distance from an exterior point to the surface.
  G4double ExactDistanceToIn(const G4ThreeVector& p, ClassifierCache& classifierCache) const;

  /// Exact shortest distance from a point to the surface.
  G4double ExactDistanceToOut(const G4ThreeVector& p) const;

  /// Compute and cache the solid volume for the current shape generation.
  G4double GetCubicVolume();

  /// Compute and cache the solid surface area for the current shape generation.
  G4double GetSurfaceArea();

  /**
   * Sample a point on the tessellated surface.
   *
   * @param diagnosticName Optional caller-provided identifier used only in
   * fatal diagnostic messages.
   */
  G4ThreeVector GetPointOnSurface(const char* diagnosticName = nullptr) const;

  /// Build or return the shared surface-sampling cache for the current shape.
  const SurfaceSamplingCache& GetOrBuildSurfaceCache() const;

private:
  /// Per-face analytical and acceleration metadata derived from `fShape`.
  struct FaceBounds {
    TopoDS_Face face;
    Bnd_Box box;
    BRepAdaptor_Surface adaptor;
    std::optional<gp_Pln> plane;
    std::vector<gp_Pnt2d> uvPolygon;
    std::optional<G4ThreeVector> outwardNormal;
  };

  /// Result bundle for closest-face queries used by normals and exact distances.
  struct ClosestFaceMatch {
    TopoDS_Face face;
    G4double distance{Infinity()};
    std::size_t faceIndex{0};
    std::optional<std::pair<Standard_Real, Standard_Real>> uv;
  };

  /// Return a classifier cache initialised for the current shape generation.
  BRepClass3d_SolidClassifier& GetOrCreateClassifier(ClassifierCache& cache) const;

  /// Return per-face intersectors initialised for the current shape generation.
  IntersectorCache& GetOrCreateIntersector(IntersectorCache& cache) const;

  /// Seed or rebuild the per-thread inscribed-sphere cache for the current shape.
  SphereCacheData& GetOrInitSphereCache(SphereCacheData& cache) const;

  /// Add a candidate inscribed sphere if it improves the current cache.
  void TryInsertSphere(SphereCacheData& cache, const G4ThreeVector& centre, G4double d) const;

  /// Recompute bounds, per-face metadata, and BVH/tessellation state from `fShape`.
  void ComputeBounds();

  /// Seed the shared set of initial inscribed spheres from the current bounds.
  void ComputeInitialSpheres();

  /// O(1) lower bound from the cached axis-aligned bounding box.
  G4double AABBLowerBound(const G4ThreeVector& p) const;

  /// BVH-backed conservative lower bound from the tessellated surface.
  G4double BVHLowerBoundDistance(const G4ThreeVector& p) const;

  /// Analytical lower bound for all-planar solids.
  G4double PlanarFaceLowerBoundDistance(const G4ThreeVector& p) const;

  /// Find the closest candidate face, optionally pruning by @p maxDistance.
  static std::optional<ClosestFaceMatch>
  TryFindClosestFace(const std::vector<FaceBounds>& faceBoundsCache, const G4ThreeVector& point,
                     G4double maxDistance = Infinity());

  /// OCCT shape wrapped by the kernel.
  TopoDS_Shape fShape;
  /// Cached axis-aligned bounds for `fShape`.
  AxisAlignedBounds fCachedBounds;
  /// Per-face derived metadata used by normals, distances, and ray tests.
  std::vector<FaceBounds> fFaceBoundsCache;
  /// Shared initial inscribed spheres copied into per-thread sphere caches.
  std::vector<InscribedSphere> fInitialSpheres;
  /// Shared tessellated triangle set used by BVH-backed queries.
  Handle(BRepExtrema_TriangleSet) fTriangleSet;
  /// Global deflection-derived tolerance for BVH lower-bound corrections.
  G4double fBVHDeflection{0.0};
  /// Per-face tessellation deflections used to conservatively adjust BVH results.
  std::vector<G4double> fFaceDeflections;
  /// True when every face in `fShape` is planar.
  bool fAllFacesPlanar{false};
  /// Monotonic generation counter for invalidating adapter-owned thread caches.
  std::atomic<std::uint64_t> fShapeGeneration{0};
  /// Lazily computed cached solid volume.
  mutable std::optional<G4double> fCachedVolume;
  /// Lazily computed cached surface area.
  mutable std::optional<G4double> fCachedSurfaceArea;
  mutable std::mutex fVolumeAreaMutex;
  /// Shared cache for area-weighted surface sampling.
  mutable std::optional<SurfaceSamplingCache> fSurfaceCache;
  /// Generation stamp for `fSurfaceCache`.
  mutable std::uint64_t fSurfaceCacheGeneration{std::numeric_limits<std::uint64_t>::max()};
  /// True while one thread is rebuilding `fSurfaceCache`.
  mutable bool fSurfaceCacheBuilding{false};
  mutable std::mutex fSurfaceCacheMutex;
  /// Coordinates waiters while the shared surface cache is being rebuilt.
  mutable std::condition_variable fSurfaceCacheCV;
};

} // namespace g4occt::detail

#endif // G4OCCT_src_G4OCCTSolidKernel_hh
