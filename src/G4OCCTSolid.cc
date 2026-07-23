// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

/// @file G4OCCTSolid.cc
/// @brief Geant4 adapter over the shared G4OCCT solid-query kernel.

#include "G4OCCT/G4OCCTSolid.hh"

#include "G4OCCTSolidKernel.hh"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>

#include <G4AffineTransform.hh>
#include <G4BoundingEnvelope.hh>
#include <G4Cache.hh>
#include <G4Exception.hh>
#include <G4Polyhedron.hh>
#include <G4TessellatedSolid.hh>
#include <G4TriangularFacet.hh>
#include <G4VGraphicsScene.hh>
#include <G4VisExtent.hh>

#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <utility>

using g4occt::detail::G4OCCTSolidKernel;

class G4OCCTSolid::Impl {
public:
  explicit Impl(const TopoDS_Shape& shape) : kernel(shape) {}

  G4OCCTSolidKernel kernel;
  mutable G4Cache<G4OCCTSolidKernel::ClassifierCache> classifierCache;
  mutable G4Cache<G4OCCTSolidKernel::IntersectorCache> intersectorCache;
  mutable G4Cache<G4OCCTSolidKernel::SphereCacheData> sphereCache;

  mutable std::unique_ptr<G4Polyhedron> cachedPolyhedron;
  mutable std::uint64_t polyhedronGeneration{std::numeric_limits<std::uint64_t>::max()};
  mutable bool polyhedronBuilding{false};
  mutable std::mutex polyhedronMutex;
  mutable std::condition_variable polyhedronCV;
};

namespace {

constexpr Standard_Real kRelativeDeflection = 0.01;

EInside ToG4Inside(const G4OCCTSolidKernel::PointClassification classification) {
  switch (classification) {
  case G4OCCTSolidKernel::PointClassification::kInside:
    return kInside;
  case G4OCCTSolidKernel::PointClassification::kSurface:
    return kSurface;
  case G4OCCTSolidKernel::PointClassification::kOutside:
  default:
    return kOutside;
  }
}

} // namespace

G4OCCTSolid::G4OCCTSolid(const G4String& name, const TopoDS_Shape& shape)
    : G4VSolid(name), fImpl(std::make_unique<Impl>(shape)) {}

G4OCCTSolid::~G4OCCTSolid() {
  fImpl->classifierCache.Get().classifier.reset();
  fImpl->intersectorCache.Get().faceIntersectors.clear();
  fImpl->sphereCache.Get().spheres.clear();
}

G4OCCTSolid* G4OCCTSolid::FromSTEP(const G4String& name, const std::string& path) {
  STEPControl_Reader reader;
  const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
  if (status != IFSelect_RetDone) {
    throw std::runtime_error("G4OCCTSolid::FromSTEP: failed to read STEP file \"" + path + "\"");
  }

  const Standard_Integer nRoots = reader.NbRootsForTransfer();
  if (nRoots == 0) {
    throw std::runtime_error("G4OCCTSolid::FromSTEP: STEP file \"" + path + "\" contains no root "
                             "shapes");
  }
  reader.TransferRoots();

  const TopoDS_Shape shape = reader.OneShape();
  if (shape.IsNull()) {
    throw std::runtime_error("G4OCCTSolid::FromSTEP: STEP file \"" + path +
                             "\" yielded a null shape");
  }
  return new G4OCCTSolid(name, shape);
}

EInside G4OCCTSolid::Inside(const G4ThreeVector& p) const {
  return ToG4Inside(fImpl->kernel.ClassifyPoint(p, fImpl->classifierCache.Get(),
                                                fImpl->intersectorCache.Get(),
                                                fImpl->sphereCache.Get()));
}

G4ThreeVector G4OCCTSolid::SurfaceNormal(const G4ThreeVector& p) const {
  return fImpl->kernel.SurfaceNormal(p);
}

G4double G4OCCTSolid::DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v) const {
  return fImpl->kernel.DistanceToIn(p, v, fImpl->intersectorCache.Get());
}

G4double G4OCCTSolid::DistanceToIn(const G4ThreeVector& p) const {
  return fImpl->kernel.DistanceToIn(p, fImpl->classifierCache.Get());
}

G4double G4OCCTSolid::DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                                    const G4bool calcNorm, G4bool* validNorm,
                                    G4ThreeVector* n) const {
  return fImpl->kernel.DistanceToOut(p, v, fImpl->intersectorCache.Get(), calcNorm, validNorm, n);
}

G4double G4OCCTSolid::DistanceToOut(const G4ThreeVector& p) const {
  return fImpl->kernel.DistanceToOut(p, fImpl->sphereCache.Get());
}

G4ThreeVector G4OCCTSolid::GetPointOnSurface() const { return fImpl->kernel.GetPointOnSurface(); }

G4double G4OCCTSolid::ExactDistanceToIn(const G4ThreeVector& p) const {
  return fImpl->kernel.ExactDistanceToIn(p, fImpl->classifierCache.Get());
}

G4double G4OCCTSolid::ExactDistanceToOut(const G4ThreeVector& p) const {
  return fImpl->kernel.ExactDistanceToOut(p);
}

G4double G4OCCTSolid::GetCubicVolume() { return fImpl->kernel.GetCubicVolume(); }

G4double G4OCCTSolid::GetSurfaceArea() { return fImpl->kernel.GetSurfaceArea(); }

G4GeometryType G4OCCTSolid::GetEntityType() const { return "G4OCCTSolid"; }

G4VisExtent G4OCCTSolid::GetExtent() const {
  const auto& bounds = fImpl->kernel.Bounds();
  return {bounds.min.x(), bounds.max.x(), bounds.min.y(), bounds.max.y(), bounds.min.z(),
          bounds.max.z()};
}

void G4OCCTSolid::BoundingLimits(G4ThreeVector& pMin, G4ThreeVector& pMax) const {
  const auto& bounds = fImpl->kernel.Bounds();
  pMin               = bounds.min;
  pMax               = bounds.max;
}

G4bool G4OCCTSolid::CalculateExtent(const EAxis pAxis, const G4VoxelLimits& pVoxelLimit,
                                    const G4AffineTransform& pTransform, G4double& pMin,
                                    G4double& pMax) const {
  const auto& bounds = fImpl->kernel.Bounds();
  const G4BoundingEnvelope envelope(bounds.min, bounds.max);
  return envelope.CalculateExtent(pAxis, pVoxelLimit, G4Transform3D(pTransform), pMin, pMax);
}

void G4OCCTSolid::DescribeYourselfTo(G4VGraphicsScene& scene) const { scene.AddSolid(*this); }

G4Polyhedron* G4OCCTSolid::CreatePolyhedron() const {
  const auto currentGeneration = fImpl->kernel.ShapeGeneration();

  {
    std::unique_lock<std::mutex> lock(fImpl->polyhedronMutex);
    fImpl->polyhedronCV.wait(lock, [this, currentGeneration] {
      return !fImpl->polyhedronBuilding || fImpl->polyhedronGeneration == currentGeneration;
    });
    if (fImpl->cachedPolyhedron && fImpl->polyhedronGeneration == currentGeneration) {
      return new G4Polyhedron(*fImpl->cachedPolyhedron);
    }
    fImpl->polyhedronBuilding = true;
  }

  BRepMesh_IncrementalMesh mesher(fImpl->kernel.Shape(), kRelativeDeflection,
                                  /*isRelative=*/Standard_True);
  (void)mesher;

  G4TessellatedSolid tessellatedSolid(GetName() + "_polyhedron");
  G4int facetCount = 0;

  for (TopExp_Explorer explorer(fImpl->kernel.Shape(), TopAbs_FACE); explorer.More();
       explorer.Next()) {
    const TopoDS_Face& face = TopoDS::Face(explorer.Current());
    TopLoc_Location location;
    const Handle(Poly_Triangulation) & triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }

    const gp_Trsf& transform  = location.Transformation();
    const bool reverseWinding = face.Orientation() == TopAbs_REVERSED;

    for (Standard_Integer triangleIndex = 1; triangleIndex <= triangulation->NbTriangles();
         ++triangleIndex) {
      Standard_Integer index1 = 0;
      Standard_Integer index2 = 0;
      Standard_Integer index3 = 0;
      triangulation->Triangle(triangleIndex).Get(index1, index2, index3);

      if (reverseWinding) {
        std::swap(index2, index3);
      }

      const gp_Pnt point1 = triangulation->Node(index1).Transformed(transform);
      const gp_Pnt point2 = triangulation->Node(index2).Transformed(transform);
      const gp_Pnt point3 = triangulation->Node(index3).Transformed(transform);

      auto* facet =
          new G4TriangularFacet(G4ThreeVector(point1.X(), point1.Y(), point1.Z()),
                                G4ThreeVector(point2.X(), point2.Y(), point2.Z()),
                                G4ThreeVector(point3.X(), point3.Y(), point3.Z()), ABSOLUTE);
      if (!facet->IsDefined() || !tessellatedSolid.AddFacet(facet)) {
        delete facet;
        continue;
      }
      ++facetCount;
    }
  }

  std::unique_ptr<G4Polyhedron> freshPolyhedron;
  if (facetCount > 0) {
    tessellatedSolid.SetSolidClosed(true);
    G4Polyhedron* tmp = tessellatedSolid.GetPolyhedron();
    if (tmp != nullptr) {
      freshPolyhedron = std::make_unique<G4Polyhedron>(*tmp);
    }
  }

  {
    std::unique_lock<std::mutex> lock(fImpl->polyhedronMutex);
    bool cacheWritten = false;
    if (freshPolyhedron && fImpl->kernel.ShapeGeneration() == currentGeneration) {
      fImpl->cachedPolyhedron   = std::make_unique<G4Polyhedron>(*freshPolyhedron);
      fImpl->polyhedronGeneration = currentGeneration;
      cacheWritten              = true;
    }
    fImpl->polyhedronBuilding = false;
    fImpl->polyhedronCV.notify_all();
    if (cacheWritten) {
      return new G4Polyhedron(*fImpl->cachedPolyhedron);
    }
  }

  return freshPolyhedron ? new G4Polyhedron(*freshPolyhedron) : nullptr;
}

std::ostream& G4OCCTSolid::StreamInfo(std::ostream& os) const {
  os << "-----------------------------------------------------------\n"
     << "    *** Dump for solid - " << GetName() << " ***\n"
     << "    ===================================================\n"
     << " Solid type: G4OCCTSolid\n"
     << " OCCT shape type: " << fImpl->kernel.Shape().ShapeType() << "\n"
     << "-----------------------------------------------------------\n";
  return os;
}

const TopoDS_Shape& G4OCCTSolid::GetOCCTShape() const { return fImpl->kernel.Shape(); }

void G4OCCTSolid::SetOCCTShape(const TopoDS_Shape& shape) {
  fImpl->kernel.SetShape(shape);
  std::unique_lock<std::mutex> lock(fImpl->polyhedronMutex);
  fImpl->cachedPolyhedron.reset();
}
