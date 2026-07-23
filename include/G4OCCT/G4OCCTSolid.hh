// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file G4OCCTSolid.hh
/// @brief Declaration of G4OCCTSolid.

#ifndef G4OCCT_G4OCCTSolid_hh
#define G4OCCT_G4OCCTSolid_hh

#include <G4ThreeVector.hh>
#include <G4VSolid.hh>

#include <iosfwd>
#include <memory>
#include <string>

class TopoDS_Shape;
class G4VGraphicsScene;

/**
 * @brief Geant4 solid wrapping an Open CASCADE Technology (OCCT) TopoDS_Shape.
 *
 * `G4OCCTSolid` is the Geant4-facing adapter over G4OCCT's shared OCCT solid
 * query kernel. The adapter preserves the original `G4VSolid` contract and
 * keeps Geant4 worker-thread cache ownership local to the Geant4 workflow,
 * while the shared kernel owns the reusable OCCT geometry/query state needed
 * for future non-Geant4 frontends.
 */
class G4OCCTSolid : public G4VSolid {
public:
  /**
   * Construct with a Geant4 solid name and an OCCT shape.
   *
   * @param name Name registered with the Geant4 solid store.
   * @param shape OCCT boundary-representation shape to wrap.
   * @throws std::invalid_argument if @p shape is null or has no computable bounding box.
   */
  G4OCCTSolid(const G4String& name, const TopoDS_Shape& shape);
  ~G4OCCTSolid() override;

  /**
   * Load a STEP file and construct a G4OCCTSolid from the first shape found.
   *
   * @param name Name registered with the Geant4 solid store.
   * @param path Filesystem path to the STEP file.
   * @return Pointer to a newly heap-allocated G4OCCTSolid (owned by the caller).
   * @throws std::runtime_error if the file cannot be read, transfers no roots,
   * or yields a null shape.
   */
  static G4OCCTSolid* FromSTEP(const G4String& name, const std::string& path);

  /// Return kInside, kSurface, or kOutside for point @p p.
  EInside Inside(const G4ThreeVector& p) const override;

  /// Return the outward unit normal at surface point @p p.
  G4ThreeVector SurfaceNormal(const G4ThreeVector& p) const override;

  /// Distance from external point @p p along direction @p v to the surface.
  G4double DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v) const override;

  /// Lower bound on the shortest distance from external point @p p to the surface.
  G4double DistanceToIn(const G4ThreeVector& p) const override;

  /// Distance from internal point @p p along direction @p v to the surface.
  G4double DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                         const G4bool calcNorm = false, G4bool* validNorm = nullptr,
                         G4ThreeVector* n = nullptr) const override;

  /// Lower bound on the shortest distance from internal point @p p to the surface.
  G4double DistanceToOut(const G4ThreeVector& p) const override;

  /// Return a point sampled on the solid surface.
  G4ThreeVector GetPointOnSurface() const override;

  /// Exact shortest distance from external point @p p to the surface.
  G4double ExactDistanceToIn(const G4ThreeVector& p) const;

  /// Exact shortest distance from internal point @p p to the surface.
  G4double ExactDistanceToOut(const G4ThreeVector& p) const;

  /// Compute and return the cubic volume of the solid.
  G4double GetCubicVolume() override;

  /// Compute and return the surface area of the solid.
  G4double GetSurfaceArea() override;

  /// Return a string identifying the entity type.
  G4GeometryType GetEntityType() const override;

  /// Return the axis-aligned bounding-box extent.
  G4VisExtent GetExtent() const override;

  /// Return axis-aligned bounding-box limits.
  void BoundingLimits(G4ThreeVector& pMin, G4ThreeVector& pMax) const override;

  /// Calculate the solid extent along axis @p pAxis under @p pTransform and @p pVoxelLimit.
  G4bool CalculateExtent(const EAxis pAxis, const G4VoxelLimits& pVoxelLimit,
                         const G4AffineTransform& pTransform, G4double& pMin,
                         G4double& pMax) const override;

  /// Describe the solid to the graphics scene.
  void DescribeYourselfTo(G4VGraphicsScene& scene) const override;

  /// Create a polyhedron representation for visualisation.
  G4Polyhedron* CreatePolyhedron() const override;

  /// Stream a human-readable description.
  std::ostream& StreamInfo(std::ostream& os) const override;

  /**
   * Read-only access to the underlying OCCT shape currently used for queries.
   */
  const TopoDS_Shape& GetOCCTShape() const;

  /**
   * Replace the underlying OCCT shape.
   *
   * @note Updating the shape bumps the kernel generation so each worker-thread
   * `G4Cache` entry is rebuilt lazily on the next query. This call is not
   * synchronised with in-flight navigation; do not call it while a simulation
   * run is in progress.
   *
   * @throws std::invalid_argument if @p shape is null or has no computable
   * bounding box.
   */
  void SetOCCTShape(const TopoDS_Shape& shape);

private:
  class Impl;
  std::unique_ptr<Impl> fImpl;
};

#endif // G4OCCT_G4OCCTSolid_hh
