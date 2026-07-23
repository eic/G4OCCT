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
  G4OCCTSolid(const G4String& name, const TopoDS_Shape& shape);
  ~G4OCCTSolid() override;

  static G4OCCTSolid* FromSTEP(const G4String& name, const std::string& path);

  EInside Inside(const G4ThreeVector& p) const override;
  G4ThreeVector SurfaceNormal(const G4ThreeVector& p) const override;
  G4double DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v) const override;
  G4double DistanceToIn(const G4ThreeVector& p) const override;
  G4double DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                         const G4bool calcNorm = false, G4bool* validNorm = nullptr,
                         G4ThreeVector* n = nullptr) const override;
  G4double DistanceToOut(const G4ThreeVector& p) const override;
  G4ThreeVector GetPointOnSurface() const override;

  G4double ExactDistanceToIn(const G4ThreeVector& p) const;
  G4double ExactDistanceToOut(const G4ThreeVector& p) const;

  G4double GetCubicVolume() override;
  G4double GetSurfaceArea() override;
  G4GeometryType GetEntityType() const override;
  G4VisExtent GetExtent() const override;
  void BoundingLimits(G4ThreeVector& pMin, G4ThreeVector& pMax) const override;
  G4bool CalculateExtent(const EAxis pAxis, const G4VoxelLimits& pVoxelLimit,
                         const G4AffineTransform& pTransform, G4double& pMin,
                         G4double& pMax) const override;
  void DescribeYourselfTo(G4VGraphicsScene& scene) const override;
  G4Polyhedron* CreatePolyhedron() const override;
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
   * @throws std::invalid_argument if @p shape is null.
   */
  void SetOCCTShape(const TopoDS_Shape& shape);

private:
  class Impl;
  std::unique_ptr<Impl> fImpl;
};

#endif // G4OCCT_G4OCCTSolid_hh
