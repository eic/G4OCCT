// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include "G4OCCTSolidKernel.hh"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepExtrema_SupportType.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BVH_Distance.hxx>
#include <BVH_Tools.hxx>
#include <ElSLib.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_Surface.hxx>
#include <NCollection_Vector.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_State.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>

#include <G4Exception.hh>
#include <G4GeometryTolerance.hh>
#include <Randomize.hh>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

constexpr Standard_Real kRelativeDeflection = 0.01;

class PointToMeshDistance
    : public BVH_Distance<Standard_Real, 3, BVH_Vec3d, BRepExtrema_TriangleSet> {
public:
  Standard_Boolean RejectNode(const BVH_Vec3d& theCornerMin, const BVH_Vec3d& theCornerMax,
                              Standard_Real& theMetric) const override {
    theMetric =
        BVH_Tools<Standard_Real, 3>::PointBoxSquareDistance(myObject, theCornerMin, theCornerMax);
    return RejectMetric(theMetric);
  }

  Standard_Boolean Accept(const Standard_Integer theIndex, const Standard_Real&) override {
    BVH_Vec3d v0, v1, v2;
    myBVHSet->GetVertices(theIndex, v0, v1, v2);
    const Standard_Real sq =
        BVH_Tools<Standard_Real, 3>::PointTriangleSquareDistance(myObject, v0, v1, v2);
    if (sq < myDistance) {
      myDistance  = sq;
      myBestIndex = theIndex;
      return Standard_True;
    }
    return Standard_False;
  }

  Standard_Integer BestIndex() const { return myBestIndex; }

private:
  Standard_Integer myBestIndex{-1};
};

class TriangleRayCast
    : public BVH_Traverse<Standard_Real, 3, BRepExtrema_TriangleSet, Standard_Real> {
public:
  void SetRay(const BVH_Vec3d& theOrigin, const BVH_Vec3d& theDir, Standard_Real theTolerance) {
    myOrigin    = theOrigin;
    myDir       = theDir;
    myTolerance = theTolerance;
    myCrossings = 0;
    myOnSurface = Standard_False;
    myDegenerate = Standard_False;
  }

  Standard_Boolean RejectNode(const BVH_Vec3d& theCornerMin, const BVH_Vec3d& theCornerMax,
                              Standard_Real& theMetric) const override {
    Standard_Real tmin = 0.0;
    Standard_Real tmax = Precision::Infinite();
    for (int k = 0; k < 3; ++k) {
      const Standard_Real dk     = (k == 0) ? myDir.x() : (k == 1) ? myDir.y() : myDir.z();
      const Standard_Real ok     = (k == 0) ? myOrigin.x() : (k == 1) ? myOrigin.y() : myOrigin.z();
      const Standard_Real ck_min = (k == 0)   ? theCornerMin.x()
                                   : (k == 1) ? theCornerMin.y()
                                              : theCornerMin.z();
      const Standard_Real ck_max = (k == 0)   ? theCornerMax.x()
                                   : (k == 1) ? theCornerMax.y()
                                              : theCornerMax.z();
      if (std::abs(dk) < Precision::Confusion()) {
        if (ok < ck_min - myTolerance || ok > ck_max + myTolerance) {
          return Standard_True;
        }
      } else {
        Standard_Real t1 = (ck_min - ok) / dk;
        Standard_Real t2 = (ck_max - ok) / dk;
        if (t1 > t2) {
          std::swap(t1, t2);
        }
        if (t1 > tmin) {
          tmin = t1;
        }
        if (t2 < tmax) {
          tmax = t2;
        }
        if (tmin > tmax + myTolerance) {
          return Standard_True;
        }
      }
    }
    theMetric = tmin;
    return Standard_False;
  }

  Standard_Boolean Accept(const Standard_Integer theIndex, const Standard_Real&) override {
    BVH_Vec3d v0, v1, v2;
    myBVHSet->GetVertices(theIndex, v0, v1, v2);
    const BVH_Vec3d edge1 = v1 - v0;
    const BVH_Vec3d edge2 = v2 - v0;
    const BVH_Vec3d h     = BVH_Vec3d::Cross(myDir, edge2);
    const Standard_Real a = edge1.Dot(h);
    if (std::abs(a) < 1e-12) {
      return Standard_False;
    }
    const Standard_Real f = 1.0 / a;
    const BVH_Vec3d s     = myOrigin - v0;
    const Standard_Real u = f * s.Dot(h);
    if (u < 0.0 || u > 1.0) {
      return Standard_False;
    }
    const BVH_Vec3d q     = BVH_Vec3d::Cross(s, edge1);
    const Standard_Real v = f * myDir.Dot(q);
    if (v < 0.0 || u + v > 1.0) {
      return Standard_False;
    }
    const Standard_Real t = f * edge2.Dot(q);
    if (std::abs(t) <= myTolerance) {
      myOnSurface = Standard_True;
      return Standard_True;
    }
    if (t < -myTolerance) {
      return Standard_False;
    }
    ++myCrossings;
    constexpr Standard_Real kEdgeTol = 1e-6;
    if (u < kEdgeTol || v < kEdgeTol || (1.0 - u - v) < kEdgeTol) {
      myDegenerate = Standard_True;
    }
    return Standard_True;
  }

  Standard_Boolean RejectMetric(const Standard_Real&) const override { return Standard_False; }
  Standard_Boolean Stop() const override { return Standard_False; }

  int Crossings() const { return myCrossings; }
  Standard_Boolean OnSurface() const { return myOnSurface; }
  Standard_Boolean Degenerate() const { return myDegenerate; }

private:
  BVH_Vec3d myOrigin;
  BVH_Vec3d myDir;
  Standard_Real myTolerance{1e-7};
  int myCrossings{0};
  Standard_Boolean myOnSurface{Standard_False};
  Standard_Boolean myDegenerate{Standard_False};
};

gp_Pnt ToPoint(const G4ThreeVector& point) { return gp_Pnt(point.x(), point.y(), point.z()); }

G4double IntersectionTolerance() {
  return 0.5 * G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
}

TopoDS_Vertex MakeVertex(const G4ThreeVector& point) {
  BRep_Builder builder;
  TopoDS_Vertex vertex;
  builder.MakeVertex(vertex, ToPoint(point), IntersectionTolerance());
  return vertex;
}

g4occt::detail::G4OCCTSolidKernel::PointClassification
ToPointClassification(const TopAbs_State state) {
  using PointClassification = g4occt::detail::G4OCCTSolidKernel::PointClassification;
  switch (state) {
  case TopAbs_IN:
    return PointClassification::kInside;
  case TopAbs_ON:
    return PointClassification::kSurface;
  case TopAbs_OUT:
  default:
    return PointClassification::kOutside;
  }
}

G4ThreeVector FallbackNormal() { return G4ThreeVector(0.0, 0.0, 1.0); }

bool PointInPolygon2d(Standard_Real u, Standard_Real v, const std::vector<gp_Pnt2d>& poly) {
  const std::size_t n = poly.size();
  int crossings       = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const gp_Pnt2d& a      = poly[i];
    const gp_Pnt2d& b      = poly[(i + 1) % n];
    const Standard_Real av = a.Y();
    const Standard_Real bv = b.Y();
    if ((av <= v && bv > v) || (bv <= v && av > v)) {
      const Standard_Real uCross = a.X() + (v - av) * (b.X() - a.X()) / (bv - av);
      if (u < uCross) {
        ++crossings;
      }
    }
  }
  return (crossings % 2) == 1;
}

bool PointOnPolygonBoundary2d(Standard_Real u, Standard_Real v, const std::vector<gp_Pnt2d>& poly,
                              Standard_Real tol) {
  const Standard_Real tol2 = tol * tol;
  const std::size_t n      = poly.size();
  for (std::size_t i = 0; i < n; ++i) {
    const gp_Pnt2d& a        = poly[i];
    const gp_Pnt2d& b        = poly[(i + 1) % n];
    const Standard_Real dx   = b.X() - a.X();
    const Standard_Real dy   = b.Y() - a.Y();
    const Standard_Real len2 = dx * dx + dy * dy;
    Standard_Real px         = 0.0;
    Standard_Real py         = 0.0;
    if (len2 < 1.0e-20) {
      px = a.X();
      py = a.Y();
    } else {
      const Standard_Real t_seg =
          std::max(0.0, std::min(1.0, ((u - a.X()) * dx + (v - a.Y()) * dy) / len2));
      px = a.X() + t_seg * dx;
      py = a.Y() + t_seg * dy;
    }
    const Standard_Real dist2 = (u - px) * (u - px) + (v - py) * (v - py);
    if (dist2 <= tol2) {
      return true;
    }
  }
  return false;
}

std::optional<Standard_Real>
RayPlaneFaceHit(const gp_Lin& ray, const gp_Pln& plane, const std::vector<gp_Pnt2d>& uvPoly,
                Standard_Real tMin, Standard_Real tMax, Standard_Real tolerance,
                Standard_Real* u_out = nullptr, Standard_Real* v_out = nullptr) {
  const gp_Dir& lineDir   = ray.Direction();
  const gp_Dir& plnNormal = plane.Axis().Direction();
  const Standard_Real denom =
      plnNormal.X() * lineDir.X() + plnNormal.Y() * lineDir.Y() + plnNormal.Z() * lineDir.Z();
  if (std::abs(denom) < 1.0e-10) {
    return std::nullopt;
  }
  const gp_Pnt& orig        = ray.Location();
  const gp_Pnt& planePt     = plane.Location();
  const Standard_Real numer = plnNormal.X() * (planePt.X() - orig.X()) +
                              plnNormal.Y() * (planePt.Y() - orig.Y()) +
                              plnNormal.Z() * (planePt.Z() - orig.Z());
  const Standard_Real t     = numer / denom;
  if (t < tMin || t > tMax) {
    return std::nullopt;
  }
  const gp_Pnt hitPt(orig.X() + t * lineDir.X(), orig.Y() + t * lineDir.Y(),
                     orig.Z() + t * lineDir.Z());
  Standard_Real u = 0.0;
  Standard_Real v = 0.0;
  ElSLib::PlaneParameters(plane.Position(), hitPt, u, v);
  if (!PointInPolygon2d(u, v, uvPoly) && !PointOnPolygonBoundary2d(u, v, uvPoly, tolerance)) {
    return std::nullopt;
  }
  if (u_out != nullptr) {
    *u_out = u;
  }
  if (v_out != nullptr) {
    *v_out = v;
  }
  return t;
}

std::optional<G4ThreeVector> TryGetOutwardNormal(const BRepAdaptor_Surface& surface,
                                                 const TopoDS_Face& face, const Standard_Real u,
                                                 const Standard_Real v) {
  Standard_Real adjustedU       = u;
  Standard_Real adjustedV       = v;
  const Standard_Real tolerance = IntersectionTolerance();
  {
    const Standard_Real uFirst   = surface.FirstUParameter();
    const Standard_Real uLast    = surface.LastUParameter();
    const Standard_Real uEpsilon = std::min(
        std::max(surface.UResolution(tolerance), Precision::PConfusion()), 0.5 * (uLast - uFirst));
    if (std::abs(adjustedU - uFirst) <= uEpsilon) {
      adjustedU = std::min(uFirst + uEpsilon, uLast);
    } else if (std::abs(adjustedU - uLast) <= uEpsilon) {
      adjustedU = std::max(uLast - uEpsilon, uFirst);
    }
  }
  {
    const Standard_Real vFirst   = surface.FirstVParameter();
    const Standard_Real vLast    = surface.LastVParameter();
    const Standard_Real vEpsilon = std::min(
        std::max(surface.VResolution(tolerance), Precision::PConfusion()), 0.5 * (vLast - vFirst));
    if (std::abs(adjustedV - vFirst) <= vEpsilon) {
      adjustedV = std::min(vFirst + vEpsilon, vLast);
    } else if (std::abs(adjustedV - vLast) <= vEpsilon) {
      adjustedV = std::max(vLast - vEpsilon, vFirst);
    }
  }

  BRepLProp_SLProps props(surface, adjustedU, adjustedV, 1, tolerance);
  if (!props.IsNormalDefined()) {
    const Standard_Real vFirst = surface.FirstVParameter();
    const Standard_Real vLast  = surface.LastVParameter();
    const Standard_Real vMid   = 0.5 * (vFirst + vLast);
    const bool nearVFirst      = (adjustedV < vMid);
    const Standard_Real vRes   = std::max(surface.VResolution(tolerance), Precision::PConfusion());
    Standard_Real finalRetryV  = adjustedV;
    for (int attempt = 0; attempt < 8 && !props.IsNormalDefined(); ++attempt) {
      const Standard_Real scale = std::pow(10.0, static_cast<Standard_Real>(attempt));
      const Standard_Real nudge = scale * vRes;
      finalRetryV =
          nearVFirst ? std::min(adjustedV + nudge, vMid) : std::max(adjustedV - nudge, vMid);
      props = BRepLProp_SLProps(surface, adjustedU, finalRetryV, 1, tolerance);
    }
    if (!props.IsNormalDefined()) {
      return std::nullopt;
    }
    constexpr Standard_Real kMaxRetryVDriftFraction = 0.10;
    if (std::fabs(finalRetryV - adjustedV) > kMaxRetryVDriftFraction * (vLast - vFirst)) {
      return std::nullopt;
    }
  }

  gp_Dir faceNormal = props.Normal();
  if (face.Orientation() == TopAbs_REVERSED) {
    faceNormal.Reverse();
  }

  return G4ThreeVector(faceNormal.X(), faceNormal.Y(), faceNormal.Z());
}

} // namespace

namespace g4occt::detail {

G4OCCTSolidKernel::G4OCCTSolidKernel(const TopoDS_Shape& shape) : fShape(shape) {
  if (fShape.IsNull()) {
    throw std::invalid_argument("G4OCCTSolidKernel: shape must not be null");
  }
  ComputeBounds();
}

void G4OCCTSolidKernel::SetShape(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    throw std::invalid_argument("G4OCCTSolidKernel::SetShape: shape must not be null");
  }
  fShape = shape;
  ComputeBounds();
  {
    std::unique_lock<std::mutex> lock(fVolumeAreaMutex);
    fCachedVolume.reset();
    fCachedSurfaceArea.reset();
  }
  {
    std::unique_lock<std::mutex> lock(fSurfaceCacheMutex);
    fSurfaceCache.reset();
  }
  fShapeGeneration.fetch_add(1, std::memory_order_release);
}

void G4OCCTSolidKernel::ComputeBounds() {
  for (TopExp_Explorer faceEx(fShape, TopAbs_FACE); faceEx.More(); faceEx.Next()) {
    const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
    if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane) {
      continue;
    }
    for (TopExp_Explorer edgeEx(face, TopAbs_EDGE); edgeEx.More(); edgeEx.Next()) {
      BRepLib::BuildPCurveForEdgeOnPlane(TopoDS::Edge(edgeEx.Current()), face);
    }
  }

  Bnd_Box boundingBox;
  BRepBndLib::AddOptimal(fShape, boundingBox, /*useTriangulation=*/Standard_False);
  if (boundingBox.IsVoid()) {
    throw std::invalid_argument(
        "G4OCCTSolidKernel: shape has no computable bounding box (no geometry)");
  }

  Standard_Real xMin = 0.0;
  Standard_Real yMin = 0.0;
  Standard_Real zMin = 0.0;
  Standard_Real xMax = 0.0;
  Standard_Real yMax = 0.0;
  Standard_Real zMax = 0.0;
  boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
  fCachedBounds =
      AxisAlignedBounds{G4ThreeVector(xMin, yMin, zMin), G4ThreeVector(xMax, yMax, zMax)};

  fFaceBoundsCache.clear();
  fFaceAdaptorIndex.clear();
  G4double maxFaceDiag = 0.0;
  for (TopExp_Explorer ex(fShape, TopAbs_FACE); ex.More(); ex.Next()) {
    Bnd_Box faceBox;
    BRepBndLib::AddOptimal(ex.Current(), faceBox, /*useTriangulation=*/Standard_False);
    const TopoDS_Face& currentFace = TopoDS::Face(ex.Current());
    const std::size_t idx          = fFaceBoundsCache.size();
    BRepAdaptor_Surface adaptor(currentFace);
    std::optional<gp_Pln> maybePlane;
    std::vector<gp_Pnt2d> uvPolygon;
    std::optional<G4ThreeVector> outwardNormal;
    if (adaptor.GetType() == GeomAbs_Plane) {
      maybePlane             = adaptor.Plane();
      const gp_Ax3& pos      = maybePlane->Position();
      const TopoDS_Wire wire = BRepTools::OuterWire(currentFace);
      if (!wire.IsNull()) {
        bool allLinear = true;
        std::vector<gp_Pnt2d> poly;
        for (BRepTools_WireExplorer we(wire, currentFace); we.More(); we.Next()) {
          const BRepAdaptor_Curve ec(we.Current());
          if (ec.GetType() != GeomAbs_Line) {
            allLinear = false;
            break;
          }
          const gp_Pnt pt = BRep_Tool::Pnt(we.CurrentVertex());
          Standard_Real u = 0.0;
          Standard_Real v = 0.0;
          ElSLib::PlaneParameters(pos, pt, u, v);
          poly.emplace_back(u, v);
        }
        if (allLinear && poly.size() >= 3) {
          int wireCount = 0;
          for (TopExp_Explorer wc(currentFace, TopAbs_WIRE); wc.More(); wc.Next()) {
            ++wireCount;
            if (wireCount > 1) {
              break;
            }
          }
          if (wireCount == 1) {
            uvPolygon = std::move(poly);
            gp_Dir faceNormal = maybePlane->Axis().Direction();
            if (currentFace.Orientation() == TopAbs_REVERSED) {
              faceNormal.Reverse();
            }
            outwardNormal = G4ThreeVector(faceNormal.X(), faceNormal.Y(), faceNormal.Z());
          }
        }
      }
    }
    fFaceBoundsCache.push_back(
        {currentFace, faceBox, std::move(adaptor), std::move(maybePlane), std::move(uvPolygon),
         std::move(outwardNormal)});
    fFaceAdaptorIndex[currentFace.TShape().get()].push_back(idx);
    if (!faceBox.IsVoid()) {
      Standard_Real fx0 = 0.0;
      Standard_Real fy0 = 0.0;
      Standard_Real fz0 = 0.0;
      Standard_Real fx1 = 0.0;
      Standard_Real fy1 = 0.0;
      Standard_Real fz1 = 0.0;
      faceBox.Get(fx0, fy0, fz0, fx1, fy1, fz1);
      const G4double diag = G4ThreeVector(fx1 - fx0, fy1 - fy0, fz1 - fz0).mag();
      maxFaceDiag         = std::max(maxFaceDiag, diag);
    }
  }

  fBVHDeflection = kRelativeDeflection * maxFaceDiag;
  fAllFacesPlanar = std::all_of(fFaceBoundsCache.begin(), fFaceBoundsCache.end(),
                                [](const FaceBounds& fb) { return fb.plane.has_value(); });

  {
    [[maybe_unused]] const BRepMesh_IncrementalMesh mesher(fShape, kRelativeDeflection,
                                                           /*isRelative=*/Standard_True);
  }

  NCollection_Vector<TopoDS_Shape> faces;
  for (TopExp_Explorer ex(fShape, TopAbs_FACE); ex.More(); ex.Next()) {
    faces.Append(ex.Current());
  }
  if (faces.IsEmpty()) {
    fTriangleSet.Nullify();
    fFaceDeflections.clear();
  } else {
    fTriangleSet = new BRepExtrema_TriangleSet(faces);
    fFaceDeflections.clear();
    fFaceDeflections.reserve(fFaceBoundsCache.size());
    for (const FaceBounds& fb : fFaceBoundsCache) {
      G4double deflection = fBVHDeflection;
      if (!fb.box.IsVoid()) {
        Standard_Real fx0 = 0.0, fy0 = 0.0, fz0 = 0.0;
        Standard_Real fx1 = 0.0, fy1 = 0.0, fz1 = 0.0;
        fb.box.Get(fx0, fy0, fz0, fx1, fy1, fz1);
        const G4double faceDiag = G4ThreeVector(fx1 - fx0, fy1 - fy0, fz1 - fz0).mag();
        deflection              = kRelativeDeflection * faceDiag;
      }
      fFaceDeflections.push_back(deflection);
    }
  }

  ComputeInitialSpheres();
}

void G4OCCTSolidKernel::ComputeInitialSpheres() {
  fInitialSpheres.clear();

  const G4double tol          = IntersectionTolerance();
  const G4ThreeVector& bmin   = fCachedBounds.min;
  const G4ThreeVector& bmax   = fCachedBounds.max;
  const G4ThreeVector centre  = 0.5 * (bmin + bmax);
  const G4ThreeVector halfExt = 0.5 * (bmax - bmin);

  std::vector<G4ThreeVector> candidates;
  candidates.reserve(15);
  candidates.push_back(centre);
  for (const G4double s : {-0.5, 0.5}) {
    candidates.push_back(centre + G4ThreeVector(s * halfExt.x(), 0.0, 0.0));
    candidates.push_back(centre + G4ThreeVector(0.0, s * halfExt.y(), 0.0));
    candidates.push_back(centre + G4ThreeVector(0.0, 0.0, s * halfExt.z()));
  }
  for (const int sx : {-1, 1}) {
    for (const int sy : {-1, 1}) {
      for (const int sz : {-1, 1}) {
        candidates.push_back(centre + G4ThreeVector(0.75 * sx * halfExt.x(),
                                                    0.75 * sy * halfExt.y(),
                                                    0.75 * sz * halfExt.z()));
      }
    }
  }

  BRepClass3d_SolidClassifier localClassifier;
  localClassifier.Load(fShape);

  for (const G4ThreeVector& cand : candidates) {
    localClassifier.Perform(ToPoint(cand), tol);
    if (localClassifier.State() != TopAbs_IN) {
      continue;
    }
    G4double d = BVHLowerBoundDistance(cand);
    if (d >= G4OCCTSolidKernel::Infinity() || d <= tol) {
      const auto match = TryFindClosestFace(fFaceBoundsCache, cand);
      if (!match.has_value() || match->distance <= tol) {
        continue;
      }
      d = match->distance;
    }
    fInitialSpheres.push_back({cand, d});
  }
  std::sort(fInitialSpheres.begin(), fInitialSpheres.end(),
            [](const InscribedSphere& a, const InscribedSphere& b) { return a.radius > b.radius; });
}

std::optional<G4OCCTSolidKernel::ClosestFaceMatch>
G4OCCTSolidKernel::TryFindClosestFace(const std::vector<FaceBounds>& faceBoundsCache,
                                      const G4ThreeVector& point, G4double maxDistance) {
  if (faceBoundsCache.empty()) {
    return std::nullopt;
  }

  const gp_Pnt queryPoint         = ToPoint(point);
  const TopoDS_Vertex queryVertex = MakeVertex(point);
  Bnd_Box queryBox;
  queryBox.Add(queryPoint);

  std::optional<ClosestFaceMatch> bestMatch;
  for (std::size_t i = 0; i < faceBoundsCache.size(); ++i) {
    const FaceBounds& fb = faceBoundsCache[i];
    const G4double threshold = bestMatch.has_value() ? bestMatch->distance : maxDistance;
    if (threshold < G4OCCTSolidKernel::Infinity() && fb.box.Distance(queryBox) > threshold) {
      continue;
    }

    BRepExtrema_DistShapeShape distance(queryVertex, fb.face);
    if (!distance.IsDone() || distance.NbSolution() == 0) {
      continue;
    }
    const G4double candidateDistance = distance.Value();

    if (bestMatch.has_value() && candidateDistance >= bestMatch->distance) {
      continue;
    }
    ClosestFaceMatch match{.face = fb.face, .distance = candidateDistance, .faceIndex = i};
    if (distance.NbSolution() > 0 && distance.SupportTypeShape2(1) == BRepExtrema_IsInFace) {
      Standard_Real u = 0.0;
      Standard_Real v = 0.0;
      distance.ParOnFaceS2(1, u, v);
      match.uv = std::make_pair(u, v);
    }
    bestMatch = std::move(match);
  }

  return bestMatch;
}

BRepClass3d_SolidClassifier&
G4OCCTSolidKernel::GetOrCreateClassifier(ClassifierCache& cache) const {
  const std::uint64_t currentGen = fShapeGeneration.load(std::memory_order_acquire);
  if (cache.generation != currentGen) {
    cache.classifier.emplace();
    cache.classifier->Load(fShape);
    cache.generation = currentGen;
  }
  return *cache.classifier;
}

G4OCCTSolidKernel::IntersectorCache&
G4OCCTSolidKernel::GetOrCreateIntersector(IntersectorCache& cache) const {
  const std::uint64_t currentGen = fShapeGeneration.load(std::memory_order_acquire);
  if (cache.generation != currentGen) {
    const G4double tol = IntersectionTolerance();
    cache.faceIntersectors.clear();
    cache.faceIntersectors.reserve(fFaceBoundsCache.size());
    cache.expandedBoxes.clear();
    cache.expandedBoxes.reserve(fFaceBoundsCache.size());
    for (const auto& fb : fFaceBoundsCache) {
      cache.faceIntersectors.push_back(std::make_unique<IntCurvesFace_Intersector>(fb.face, tol));
      Bnd_Box expanded = fb.box;
      expanded.Enlarge(tol);
      cache.expandedBoxes.push_back(std::move(expanded));
    }
    cache.generation = currentGen;
  }
  return cache;
}

G4OCCTSolidKernel::SphereCacheData&
G4OCCTSolidKernel::GetOrInitSphereCache(SphereCacheData& cache) const {
  const std::uint64_t currentGen = fShapeGeneration.load(std::memory_order_acquire);
  if (cache.generation != currentGen) {
    cache.spheres    = fInitialSpheres;
    cache.generation = currentGen;
  }
  return cache;
}

void G4OCCTSolidKernel::TryInsertSphere(SphereCacheData& cache, const G4ThreeVector& centre,
                                        G4double d) const {
  if (d <= 0.0) {
    return;
  }
  const G4double minRadius = IntersectionTolerance();
  if (d <= minRadius) {
    return;
  }
  cache = GetOrInitSphereCache(cache);

  if (cache.spheres.size() >= kMaxInscribedSpheres && d <= cache.spheres.back().radius) {
    return;
  }

  for (const InscribedSphere& s : cache.spheres) {
    if (s.radius >= d) {
      const G4double gap = s.radius - d;
      if ((centre - s.centre).mag2() <= gap * gap) {
        return;
      }
    }
  }

  const InscribedSphere newSphere{centre, d};
  const auto it = std::lower_bound(
      cache.spheres.begin(), cache.spheres.end(), newSphere,
      [](const InscribedSphere& a, const InscribedSphere& b) { return a.radius > b.radius; });
  cache.spheres.insert(it, newSphere);
  if (cache.spheres.size() > kMaxInscribedSpheres) {
    cache.spheres.pop_back();
  }
}

G4OCCTSolidKernel::PointClassification
G4OCCTSolidKernel::ClassifyPoint(const G4ThreeVector& p, ClassifierCache& classifierCache,
                                 IntersectorCache& intersectorCache,
                                 SphereCacheData& sphereCache) const {
  const G4double tolerance = IntersectionTolerance();
  if (p.x() < fCachedBounds.min.x() - tolerance || p.x() > fCachedBounds.max.x() + tolerance ||
      p.y() < fCachedBounds.min.y() - tolerance || p.y() > fCachedBounds.max.y() + tolerance ||
      p.z() < fCachedBounds.min.z() - tolerance || p.z() > fCachedBounds.max.z() + tolerance) {
    return PointClassification::kOutside;
  }

  const SphereCacheData& localSphereCache = GetOrInitSphereCache(sphereCache);
  for (const InscribedSphere& s : localSphereCache.spheres) {
    const G4double interiorRadius = s.radius - tolerance;
    if (interiorRadius > 0.0 && (p - s.centre).mag2() < interiorRadius * interiorRadius) {
      return PointClassification::kInside;
    }
  }

  if (!fTriangleSet.IsNull() && fTriangleSet->Size() > 0) {
    if (fBVHDeflection > 0.0) {
      const G4double bvhLB = BVHLowerBoundDistance(p);
      if (bvhLB < tolerance) {
        auto& classifier = GetOrCreateClassifier(classifierCache);
        classifier.Perform(ToPoint(p), tolerance);
        return ToPointClassification(classifier.State());
      }
    }

    const BVH_Vec3d bvhOrigin(p.x(), p.y(), p.z());
    const Standard_Real bvhTol = static_cast<Standard_Real>(tolerance);
    TriangleRayCast caster;
    caster.SetBVHSet(fTriangleSet.get());
    caster.SetRay(bvhOrigin, BVH_Vec3d(0.0, 0.0, 1.0), bvhTol);
    caster.Select();

    if (caster.OnSurface()) {
      return PointClassification::kSurface;
    }
    if (!caster.Degenerate() && caster.Crossings() > 0) {
      return (caster.Crossings() % 2 == 1) ? PointClassification::kInside
                                           : PointClassification::kOutside;
    }

    int insideVotes  = 0;
    int outsideVotes = 0;
    if (!caster.Degenerate()) {
      if (caster.Crossings() % 2 == 1) {
        ++insideVotes;
      } else {
        ++outsideVotes;
      }
    }

    const BVH_Vec3d kExtraRays[2] = {
        BVH_Vec3d(1.0, 0.0, 0.0),
        BVH_Vec3d(0.0, 1.0, 0.0),
    };
    for (const BVH_Vec3d& dir : kExtraRays) {
      caster.SetRay(bvhOrigin, dir, bvhTol);
      caster.Select();
      if (caster.OnSurface()) {
        return PointClassification::kSurface;
      }
      if (!caster.Degenerate()) {
        if (caster.Crossings() % 2 == 1) {
          ++insideVotes;
        } else {
          ++outsideVotes;
        }
      }
    }

    if (insideVotes > outsideVotes) {
      return PointClassification::kInside;
    }
    if (outsideVotes > insideVotes) {
      return PointClassification::kOutside;
    }

    auto& classifier = GetOrCreateClassifier(classifierCache);
    classifier.Perform(ToPoint(p), tolerance);
    return ToPointClassification(classifier.State());
  }

  IntersectorCache& cache = GetOrCreateIntersector(intersectorCache);
  const gp_Lin ray(ToPoint(p), gp_Dir(0.0, 0.0, 1.0));
  int crossings      = 0;
  bool onSurface     = false;
  bool degenerateRay = false;

  for (std::size_t i = 0; i < fFaceBoundsCache.size(); ++i) {
    if (cache.expandedBoxes[i].IsOut(ray)) {
      continue;
    }
    const FaceBounds& fb = fFaceBoundsCache[i];
    if (!fb.uvPolygon.empty()) {
      Standard_Real u_hit = 0.0;
      Standard_Real v_hit = 0.0;
      const auto t        = RayPlaneFaceHit(ray, *fb.plane, fb.uvPolygon, -tolerance,
                                            Precision::Infinite(), tolerance, &u_hit, &v_hit);
      if (t) {
        const G4double w = static_cast<G4double>(*t);
        if (std::abs(w) <= tolerance) {
          onSurface = true;
        } else if (w > tolerance) {
          if (PointOnPolygonBoundary2d(u_hit, v_hit, fb.uvPolygon, tolerance)) {
            degenerateRay = true;
          } else {
            ++crossings;
          }
        }
      }
    } else {
      IntCurvesFace_Intersector& fi = *cache.faceIntersectors[i];
      fi.Perform(ray, -tolerance, Precision::Infinite());
      if (!fi.IsDone()) {
        continue;
      }
      for (Standard_Integer j = 1; j <= fi.NbPnt(); ++j) {
        const G4double w         = fi.WParameter(j);
        const TopAbs_State state = fi.State(j);
        if (std::abs(w) <= tolerance && (state == TopAbs_IN || state == TopAbs_ON)) {
          onSurface = true;
        } else if (w > tolerance && state == TopAbs_IN) {
          ++crossings;
        } else if (w > tolerance && state == TopAbs_ON) {
          degenerateRay = true;
        }
      }
    }
  }

  if (onSurface) {
    return PointClassification::kSurface;
  }
  if (crossings == 0 || degenerateRay) {
    auto& classifier = GetOrCreateClassifier(classifierCache);
    classifier.Perform(ToPoint(p), tolerance);
    return ToPointClassification(classifier.State());
  }
  return (crossings % 2 == 1) ? PointClassification::kInside : PointClassification::kOutside;
}

G4ThreeVector G4OCCTSolidKernel::SurfaceNormal(const G4ThreeVector& p) const {
  if (fAllFacesPlanar) {
    const gp_Pnt pt          = ToPoint(p);
    const FaceBounds* bestFB = nullptr;
    G4double bestDist        = G4OCCTSolidKernel::Infinity();
    for (const FaceBounds& fb : fFaceBoundsCache) {
      if (!fb.plane.has_value() || !fb.outwardNormal.has_value()) {
        continue;
      }
      const G4double d = fb.plane->Distance(pt);
      if (d < bestDist) {
        bestDist = d;
        bestFB   = &fb;
      }
    }
    if (bestFB) {
      return *bestFB->outwardNormal;
    }
  }

  const auto projectAndGetNormalFallback = [&](const FaceBounds& fb) -> G4ThreeVector {
    TopLoc_Location loc;
    const Handle(Geom_Surface) surface = BRep_Tool::Surface(fb.face, loc);
    if (surface.IsNull()) {
      return FallbackNormal();
    }
    gp_Pnt pLocal = ToPoint(p);
    if (!loc.IsIdentity()) {
      pLocal.Transform(loc.Transformation().Inverted());
    }
    GeomAPI_ProjectPointOnSurf projection(pLocal, surface);
    if (projection.NbPoints() == 0) {
      return FallbackNormal();
    }
    Standard_Real u = 0.0;
    Standard_Real v = 0.0;
    projection.LowerDistanceParameters(u, v);
    return TryGetOutwardNormal(fb.adaptor, fb.face, u, v).value_or(FallbackNormal());
  };

  const G4double bvhLB = BVHLowerBoundDistance(p);
  const G4double seedDist =
      (fBVHDeflection > 0.0 && bvhLB < G4OCCTSolidKernel::Infinity()) ? bvhLB + 2.0 * fBVHDeflection : G4OCCTSolidKernel::Infinity();
  const auto closestFaceMatch = TryFindClosestFace(fFaceBoundsCache, p, seedDist);
  if (!closestFaceMatch.has_value()) {
    return FallbackNormal();
  }
  const FaceBounds& fb = fFaceBoundsCache[closestFaceMatch->faceIndex];

  if (fb.outwardNormal.has_value()) {
    return *fb.outwardNormal;
  }
  if (closestFaceMatch->uv.has_value()) {
    const auto [u, v] = *closestFaceMatch->uv;
    const auto result = TryGetOutwardNormal(fb.adaptor, fb.face, u, v);
    if (result.has_value()) {
      return *result;
    }
  }
  return projectAndGetNormalFallback(fb);
}

G4double G4OCCTSolidKernel::DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v,
                                         IntersectorCache& intersectorCache) const {
  const G4double tolerance = IntersectionTolerance();
  const gp_Lin ray(ToPoint(p), gp_Dir(v.x(), v.y(), v.z()));
  IntersectorCache& cache = GetOrCreateIntersector(intersectorCache);

  G4double minDistance = G4OCCTSolidKernel::Infinity();
  for (std::size_t i = 0; i < fFaceBoundsCache.size(); ++i) {
    if (cache.expandedBoxes[i].IsOut(ray)) {
      continue;
    }
    const FaceBounds& fb = fFaceBoundsCache[i];
    if (!fb.uvPolygon.empty()) {
      const auto t = RayPlaneFaceHit(ray, *fb.plane, fb.uvPolygon, tolerance, Precision::Infinite(),
                                     tolerance);
      if (t && *t < minDistance) {
        minDistance = static_cast<G4double>(*t);
      }
    } else {
      IntCurvesFace_Intersector& fi = *cache.faceIntersectors[i];
      fi.Perform(ray, tolerance, Precision::Infinite());
      if (!fi.IsDone()) {
        continue;
      }
      for (Standard_Integer j = 1; j <= fi.NbPnt(); ++j) {
        const G4double w = fi.WParameter(j);
        if (w > tolerance && w < minDistance) {
          minDistance = w;
        }
      }
    }
  }
  return minDistance;
}

G4double G4OCCTSolidKernel::ExactDistanceToIn(const G4ThreeVector& p,
                                              ClassifierCache& classifierCache) const {
  auto& classifier = GetOrCreateClassifier(classifierCache);
  classifier.Perform(ToPoint(p), IntersectionTolerance());
  if (classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON) {
    return 0.0;
  }

  const auto match = TryFindClosestFace(fFaceBoundsCache, p);
  if (!match.has_value()) {
    return G4OCCTSolidKernel::Infinity();
  }
  return (match->distance <= IntersectionTolerance()) ? 0.0 : match->distance;
}

G4double G4OCCTSolidKernel::AABBLowerBound(const G4ThreeVector& p) const {
  const G4ThreeVector& mn = fCachedBounds.min;
  const G4ThreeVector& mx = fCachedBounds.max;
  const G4double dx       = std::max({0.0, mn.x() - p.x(), p.x() - mx.x()});
  const G4double dy       = std::max({0.0, mn.y() - p.y(), p.y() - mx.y()});
  const G4double dz       = std::max({0.0, mn.z() - p.z(), p.z() - mx.z()});
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

G4double G4OCCTSolidKernel::BVHLowerBoundDistance(const G4ThreeVector& p) const {
  if (fTriangleSet.IsNull() || fTriangleSet->Size() == 0) {
    return G4OCCTSolidKernel::Infinity();
  }
  PointToMeshDistance solver;
  solver.SetObject(BVH_Vec3d(p.x(), p.y(), p.z()));
  solver.SetBVHSet(fTriangleSet.get());
  const Standard_Real meshDistSq = solver.ComputeDistance();
  if (!solver.IsDone()) {
    return G4OCCTSolidKernel::Infinity();
  }
  const G4double meshDist = std::sqrt(static_cast<G4double>(meshDistSq));

  G4double deflection            = fBVHDeflection;
  const Standard_Integer bestIdx = solver.BestIndex();
  if (bestIdx >= 0) {
    const Standard_Integer faceId = fTriangleSet->GetFaceID(bestIdx);
    if (faceId >= 0 && static_cast<std::size_t>(faceId) < fFaceDeflections.size()) {
      deflection = fFaceDeflections[static_cast<std::size_t>(faceId)];
    }
  }

  return std::max(0.0, meshDist - deflection);
}

G4double G4OCCTSolidKernel::PlanarFaceLowerBoundDistance(const G4ThreeVector& p) const {
  const gp_Pnt pt  = ToPoint(p);
  G4double minDist = G4OCCTSolidKernel::Infinity();
  for (const FaceBounds& fb : fFaceBoundsCache) {
    if (!fb.plane.has_value()) {
      continue;
    }
    const G4double d = static_cast<G4double>(fb.plane->Distance(pt));
    if (d < minDist) {
      minDist = d;
    }
  }
  return minDist;
}

G4double G4OCCTSolidKernel::DistanceToIn(const G4ThreeVector& p,
                                         ClassifierCache& classifierCache) const {
  const G4double aabbDist = AABBLowerBound(p);
  if (aabbDist > IntersectionTolerance()) {
    return aabbDist;
  }

  const G4double bvhDist = BVHLowerBoundDistance(p);
  if (bvhDist < G4OCCTSolidKernel::Infinity() && bvhDist > IntersectionTolerance()) {
    return bvhDist;
  }
  return ExactDistanceToIn(p, classifierCache);
}

G4double G4OCCTSolidKernel::DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v,
                                          IntersectorCache& intersectorCache,
                                          const G4bool calcNorm, G4bool* validNorm,
                                          G4ThreeVector* n) const {
  if (validNorm != nullptr) {
    *validNorm = false;
  }

  const G4double tolerance = IntersectionTolerance();
  const gp_Lin ray(ToPoint(p), gp_Dir(v.x(), v.y(), v.z()));
  IntersectorCache& cache = GetOrCreateIntersector(intersectorCache);

  G4double minDistance   = G4OCCTSolidKernel::Infinity();
  std::size_t minFaceIdx = std::numeric_limits<std::size_t>::max();
  G4double minU          = 0.0;
  G4double minV          = 0.0;
  bool minIsIn           = false;
  bool minIsFastPath     = false;

  for (std::size_t i = 0; i < fFaceBoundsCache.size(); ++i) {
    if (cache.expandedBoxes[i].IsOut(ray)) {
      continue;
    }
    const FaceBounds& fb = fFaceBoundsCache[i];
    if (!fb.uvPolygon.empty()) {
      const auto t = RayPlaneFaceHit(ray, *fb.plane, fb.uvPolygon, tolerance, Precision::Infinite(),
                                     tolerance);
      if (t && *t < minDistance) {
        minDistance   = static_cast<G4double>(*t);
        minFaceIdx    = i;
        minIsIn       = true;
        minIsFastPath = true;
      }
    } else {
      IntCurvesFace_Intersector& fi = *cache.faceIntersectors[i];
      fi.Perform(ray, tolerance, Precision::Infinite());
      if (!fi.IsDone()) {
        continue;
      }
      for (Standard_Integer j = 1; j <= fi.NbPnt(); ++j) {
        const G4double w = fi.WParameter(j);
        if (w > tolerance && w < minDistance) {
          minDistance   = w;
          minFaceIdx    = i;
          minU          = fi.UParameter(j);
          minV          = fi.VParameter(j);
          minIsIn       = (fi.State(j) == TopAbs_IN || fi.State(j) == TopAbs_ON);
          minIsFastPath = false;
        }
      }
    }
  }

  if (minFaceIdx == std::numeric_limits<std::size_t>::max() || minDistance == G4OCCTSolidKernel::Infinity()) {
    return 0.0;
  }

  if (calcNorm && validNorm != nullptr && n != nullptr && minIsIn) {
    const FaceBounds& fb = fFaceBoundsCache[minFaceIdx];
    if (minIsFastPath && fb.outwardNormal.has_value()) {
      *n         = *fb.outwardNormal;
      *validNorm = true;
    } else {
      const auto outNorm = TryGetOutwardNormal(fb.adaptor, fb.face, minU, minV);
      if (outNorm) {
        *n         = *outNorm;
        *validNorm = true;
      }
    }
  }
  return minDistance;
}

G4double G4OCCTSolidKernel::ExactDistanceToOut(const G4ThreeVector& p) const {
  const auto match = TryFindClosestFace(fFaceBoundsCache, p);
  if (!match.has_value()) {
    return 0.0;
  }
  return (match->distance <= IntersectionTolerance()) ? 0.0 : match->distance;
}

G4double G4OCCTSolidKernel::DistanceToOut(const G4ThreeVector& p,
                                          SphereCacheData& sphereCache) const {
  G4double d;
  if (fAllFacesPlanar) {
    d = PlanarFaceLowerBoundDistance(p);
    if (d == G4OCCTSolidKernel::Infinity()) {
      d = ExactDistanceToOut(p);
    }
  } else {
    const G4double bvhDist = BVHLowerBoundDistance(p);
    d                      = (bvhDist < G4OCCTSolidKernel::Infinity()) ? bvhDist : ExactDistanceToOut(p);
  }
  TryInsertSphere(sphereCache, p, d);
  return d;
}

G4double G4OCCTSolidKernel::GetCubicVolume() {
  std::unique_lock<std::mutex> lock(fVolumeAreaMutex);
  if (!fCachedVolume) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(fShape, props);
    fCachedVolume = props.Mass();
  }
  return *fCachedVolume;
}

G4double G4OCCTSolidKernel::GetSurfaceArea() {
  std::unique_lock<std::mutex> lock(fVolumeAreaMutex);
  if (!fCachedSurfaceArea) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(fShape, props);
    fCachedSurfaceArea = props.Mass();
  }
  return *fCachedSurfaceArea;
}

const G4OCCTSolidKernel::SurfaceSamplingCache& G4OCCTSolidKernel::GetOrBuildSurfaceCache() const {
  BRepMesh_IncrementalMesh mesher(fShape, kRelativeDeflection, /*isRelative=*/Standard_True);
  (void)mesher;

  std::unique_lock<std::mutex> lock(fSurfaceCacheMutex);
  const std::uint64_t currentGen = fShapeGeneration.load(std::memory_order_acquire);
  if (fSurfaceCache.has_value() && fSurfaceCacheGeneration == currentGen) {
    return *fSurfaceCache;
  }

  SurfaceSamplingCache cache;
  for (TopExp_Explorer ex(fShape, TopAbs_FACE); ex.More(); ex.Next()) {
    const TopoDS_Face& face = TopoDS::Face(ex.Current());
    TopLoc_Location loc;
    const Handle(Poly_Triangulation) & triangulation = BRep_Tool::Triangulation(face, loc);
    if (triangulation.IsNull()) {
      continue;
    }

    const auto faceIndex = static_cast<std::uint32_t>(cache.faces.size());
    cache.faces.push_back(face);

    const gp_Trsf& transform  = loc.Transformation();
    const bool reverseWinding = face.Orientation() == TopAbs_REVERSED;

    for (Standard_Integer i = 1; i <= triangulation->NbTriangles(); ++i) {
      Standard_Integer idx1 = 0;
      Standard_Integer idx2 = 0;
      Standard_Integer idx3 = 0;
      triangulation->Triangle(i).Get(idx1, idx2, idx3);
      if (reverseWinding) {
        std::swap(idx2, idx3);
      }

      const gp_Pnt q1 = triangulation->Node(idx1).Transformed(transform);
      const gp_Pnt q2 = triangulation->Node(idx2).Transformed(transform);
      const gp_Pnt q3 = triangulation->Node(idx3).Transformed(transform);

      const G4ThreeVector v1(q1.X(), q1.Y(), q1.Z());
      const G4ThreeVector v2(q2.X(), q2.Y(), q2.Z());
      const G4ThreeVector v3(q3.X(), q3.Y(), q3.Z());

      const G4double area = 0.5 * (v2 - v1).cross(v3 - v1).mag();
      if (area > 0.0) {
        cache.totalArea += area;
        cache.cumulativeAreas.push_back(cache.totalArea);
        cache.triangles.push_back({v1, v2, v3, faceIndex});
      }
    }
  }

  fSurfaceCache           = std::move(cache);
  fSurfaceCacheGeneration = currentGen;
  return *fSurfaceCache;
}

G4ThreeVector G4OCCTSolidKernel::GetPointOnSurface() const {
  const SurfaceSamplingCache& cache = GetOrBuildSurfaceCache();

  if (cache.triangles.empty() || cache.totalArea == 0.0) {
    G4ExceptionDescription msg;
    msg << "Tessellation of the OCCT shape produced no valid triangles; cannot sample a point on "
           "the surface.";
    G4Exception("G4OCCTSolidKernel::GetPointOnSurface", "GeomMgt1001", FatalException, msg);
    return {0.0, 0.0, 0.0};
  }

  const G4double target = G4UniformRand() * cache.totalArea;
  const auto it         = std::ranges::lower_bound(cache.cumulativeAreas, target);
  const std::size_t idx = std::min(static_cast<std::size_t>(it - cache.cumulativeAreas.begin()),
                                   cache.triangles.size() - 1);
  const SurfaceTriangle& chosen = cache.triangles[idx];

  G4double r1 = G4UniformRand();
  G4double r2 = G4UniformRand();
  if (r1 + r2 > 1.0) {
    r1 = 1.0 - r1;
    r2 = 1.0 - r2;
  }

  const G4ThreeVector tessPoint =
      chosen.p1 + r1 * (chosen.p2 - chosen.p1) + r2 * (chosen.p3 - chosen.p1);

  const TopoDS_Face& face = cache.faces[chosen.faceIndex];
  TopLoc_Location loc;
  const Handle(Geom_Surface) geomSurface = BRep_Tool::Surface(face, loc);
  if (!geomSurface.IsNull()) {
    gp_Pnt tessPointLocal(tessPoint.x(), tessPoint.y(), tessPoint.z());
    if (!loc.IsIdentity()) {
      tessPointLocal.Transform(loc.Transformation().Inverted());
    }
    GeomAPI_ProjectPointOnSurf projection(tessPointLocal, geomSurface);
    if (projection.NbPoints() > 0) {
      gp_Pnt projectedPoint = projection.NearestPoint();
      if (!loc.IsIdentity()) {
        projectedPoint.Transform(loc.Transformation());
      }
      return {projectedPoint.X(), projectedPoint.Y(), projectedPoint.Z()};
    }
  }

  return tessPoint;
}

} // namespace g4occt::detail
