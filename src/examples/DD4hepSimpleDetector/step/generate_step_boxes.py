#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
"""Generate STEP boxes for the DD4hep SimpleDetector port example.

The files produced here are intentionally checked in so the example works
without requiring OpenCASCADE or a local generation step. Run this script after
changing the box dimensions and commit the updated `*.step` files.
"""

from pathlib import Path


def _fmt(value: float) -> str:
    if value == 0.0:
        return "0."
    text = f"{value:.10G}"
    if "." not in text and "E" not in text:
        return text + "."
    if "." in text:
        text = text.rstrip("0")
        if text.endswith("."):
            return text
    return text


def _fmt3(x: float, y: float, z: float) -> str:
    return f"({_fmt(x)},{_fmt(y)},{_fmt(z)})"


def _fmt2(u: float, v: float) -> str:
    return f"({_fmt(u)},{_fmt(v)})"


class StepWriter:
    def __init__(self) -> None:
        self._next_id = 0
        self._lines: list[str] = []

    def alloc(self) -> int:
        self._next_id += 1
        return self._next_id

    def add(self, entity_id: int, text: str) -> None:
        self._lines.append(f"#{entity_id} = {text};")

    def write_to(self, path: Path) -> None:
        header = [
            "ISO-10303-21;",
            "HEADER;",
            "FILE_DESCRIPTION(('Open CASCADE Model'),'2;1');",
            "FILE_NAME('Open CASCADE Shape Model','2026-07-21T00:00:00',('Author'),(",
            "    'Open CASCADE'),'Open CASCADE STEP processor 7.9','Open CASCADE 7.9'",
            "  ,'Unknown');",
            "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));",
            "ENDSEC;",
            "DATA;",
        ]
        footer = ["ENDSEC;", "END-ISO-10303-21;"]
        path.write_text("\n".join(header + self._lines + footer) + "\n", encoding="utf-8")


_GEOM2D = (
    "( GEOMETRIC_REPRESENTATION_CONTEXT(2)\n"
    "PARAMETRIC_REPRESENTATION_CONTEXT()"
    " REPRESENTATION_CONTEXT('2D SPACE',''\n"
    "  ) )"
)


def _add_geom3d_context(writer: StepWriter) -> int:
    lu = writer.alloc()
    pau = writer.alloc()
    sau = writer.alloc()
    unc = writer.alloc()
    ctx = writer.alloc()
    writer.add(lu, "( LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.) )")
    writer.add(pau, "( NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.) )")
    writer.add(sau, "( NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) SOLID_ANGLE_UNIT() )")
    writer.add(
        unc,
        f"UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-07),#{lu},\n"
        "  'distance_accuracy_value','confusion accuracy')",
    )
    writer.add(
        ctx,
        f"( GEOMETRIC_REPRESENTATION_CONTEXT(3)\n"
        f"GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#{unc}))"
        f" GLOBAL_UNIT_ASSIGNED_CONTEXT\n"
        f"((#{lu},#{pau},#{sau}))"
        f" REPRESENTATION_CONTEXT('Context #1',\n"
        f"  '3D Context with UNIT and UNCERTAINTY') )",
    )
    return ctx


def _add_rectangular_box(
    writer: StepWriter,
    half_x: float,
    half_y: float,
    half_z: float,
    geom3d_context_id: int,
    app_context_id: int,
    product_name: str,
) -> None:
    x0, x1 = -half_x, half_x
    y0, y1 = -half_y, half_y
    z0, z1 = -half_z, half_z

    vertices = [
        (x0, y0, z0),
        (x1, y0, z0),
        (x1, y1, z0),
        (x0, y1, z0),
        (x0, y0, z1),
        (x1, y0, z1),
        (x1, y1, z1),
        (x0, y1, z1),
    ]

    vertex_points = []
    for vx, vy, vz in vertices:
        cp = writer.alloc()
        vp = writer.alloc()
        writer.add(cp, f"CARTESIAN_POINT('',{_fmt3(vx, vy, vz)})")
        writer.add(vp, f"VERTEX_POINT('',#{cp})")
        vertex_points.append(vp)

    face_plane_defs = [
        ((x0, y0, z0), (1, 0, 0), (0, 0, 1)),
        ((x1, y0, z0), (1, 0, 0), (0, 0, 1)),
        ((x0, y0, z0), (0, 1, 0), (0, 0, 1)),
        ((x0, y1, z0), (0, 1, 0), (0, 0, 1)),
        ((x0, y0, z0), (0, 0, 1), (1, 0, 0)),
        ((x0, y0, z1), (0, 0, 1), (1, 0, 0)),
    ]

    plane_ids = []
    for (ox, oy, oz), (ax, ay, az), (rx, ry, rz) in face_plane_defs:
        o_id = writer.alloc()
        a_id = writer.alloc()
        r_id = writer.alloc()
        p3_id = writer.alloc()
        pl_id = writer.alloc()
        writer.add(o_id, f"CARTESIAN_POINT('',{_fmt3(ox, oy, oz)})")
        writer.add(a_id, f"DIRECTION('',{_fmt3(ax, ay, az)})")
        writer.add(r_id, f"DIRECTION('',{_fmt3(rx, ry, rz)})")
        writer.add(p3_id, f"AXIS2_PLACEMENT_3D('',#{o_id},#{a_id},#{r_id})")
        writer.add(pl_id, f"PLANE('',#{p3_id})")
        plane_ids.append(pl_id)

    geom2d = writer.alloc()
    writer.add(geom2d, _GEOM2D)

    def uv(face_index: int, vx: float, vy: float, vz: float) -> tuple[float, float]:
        if face_index in (0, 1):
            return (vz - z0, y0 - vy)
        if face_index in (2, 3):
            return (vz - z0, vx - x0)
        return (vx - x0, vy - y0)

    def make_pcurve(plane_id: int, su: float, sv: float, eu: float, ev: float) -> int:
        du, dv = eu - su, ev - sv
        length = (du * du + dv * dv) ** 0.5
        if length < 1e-12:
            raise ValueError("Zero-length edge in UV space")
        du /= length
        dv /= length
        o2d = writer.alloc()
        d2d = writer.alloc()
        v2d = writer.alloc()
        l2d = writer.alloc()
        dr = writer.alloc()
        pc = writer.alloc()
        writer.add(o2d, f"CARTESIAN_POINT('',{_fmt2(su, sv)})")
        writer.add(d2d, f"DIRECTION('',{_fmt2(du, dv)})")
        writer.add(v2d, f"VECTOR('',#{d2d},1.)")
        writer.add(l2d, f"LINE('',#{o2d},#{v2d})")
        writer.add(dr, f"DEFINITIONAL_REPRESENTATION('',(#{l2d}),#{geom2d})")
        writer.add(pc, f"PCURVE('',#{plane_id},#{dr})")
        return pc

    edge_defs = [
        (0, 4, (0, 0, 1), 0, 2),
        (0, 3, (0, 1, 0), 0, 4),
        (3, 7, (0, 0, 1), 0, 3),
        (4, 7, (0, 1, 0), 0, 5),
        (1, 5, (0, 0, 1), 1, 2),
        (1, 2, (0, 1, 0), 1, 4),
        (2, 6, (0, 0, 1), 1, 3),
        (5, 6, (0, 1, 0), 1, 5),
        (0, 1, (1, 0, 0), 2, 4),
        (4, 5, (1, 0, 0), 2, 5),
        (3, 2, (1, 0, 0), 3, 4),
        (7, 6, (1, 0, 0), 3, 5),
    ]

    edge_curves = []
    for start_index, end_index, (dx, dy, dz), face1, face2 in edge_defs:
        sx, sy, sz = vertices[start_index]
        ex, ey, ez = vertices[end_index]
        o3d = writer.alloc()
        d3d = writer.alloc()
        v3d = writer.alloc()
        l3d = writer.alloc()
        writer.add(o3d, f"CARTESIAN_POINT('',{_fmt3(sx, sy, sz)})")
        writer.add(d3d, f"DIRECTION('',{_fmt3(dx, dy, dz)})")
        writer.add(v3d, f"VECTOR('',#{d3d},1.)")
        writer.add(l3d, f"LINE('',#{o3d},#{v3d})")
        pc1 = make_pcurve(plane_ids[face1], *uv(face1, sx, sy, sz), *uv(face1, ex, ey, ez))
        pc2 = make_pcurve(plane_ids[face2], *uv(face2, sx, sy, sz), *uv(face2, ex, ey, ez))
        sc = writer.alloc()
        writer.add(sc, f"SURFACE_CURVE('',#{l3d},(#{pc1},#{pc2}),.PCURVE_S1.)")
        ec = writer.alloc()
        writer.add(
            ec,
            f"EDGE_CURVE('',#{vertex_points[start_index]},#{vertex_points[end_index]},#{sc},.T.)",
        )
        edge_curves.append(ec)

    face_loop_defs = [
        [(0, False), (1, True), (2, True), (3, False)],
        [(4, False), (5, True), (6, True), (7, False)],
        [(8, False), (0, True), (9, True), (4, False)],
        [(10, False), (2, True), (11, True), (6, False)],
        [(1, False), (8, True), (5, True), (10, False)],
        [(3, False), (9, True), (7, True), (11, False)],
    ]
    face_senses = [".F.", ".T.", ".F.", ".T.", ".F.", ".T."]

    face_ids = []
    for face_index in range(6):
        oriented_edges = []
        for edge_index, forward in face_loop_defs[face_index]:
            oe = writer.alloc()
            writer.add(oe, f"ORIENTED_EDGE('',*,*,#{edge_curves[edge_index]},{'.T.' if forward else '.F.'})")
            oriented_edges.append(oe)
        edge_loop = writer.alloc()
        writer.add(edge_loop, f"EDGE_LOOP('',({','.join('#' + str(i) for i in oriented_edges)}))")
        face_bound = writer.alloc()
        writer.add(face_bound, f"FACE_BOUND('',#{edge_loop},{face_senses[face_index]})")
        advanced_face = writer.alloc()
        writer.add(
            advanced_face,
            f"ADVANCED_FACE('',(#{face_bound}),#{plane_ids[face_index]},{face_senses[face_index]})",
        )
        face_ids.append(advanced_face)

    closed_shell = writer.alloc()
    writer.add(closed_shell, f"CLOSED_SHELL('',({','.join('#' + str(i) for i in face_ids)}))")
    manifold = writer.alloc()
    writer.add(manifold, f"MANIFOLD_SOLID_BREP('',#{closed_shell})")

    origin = writer.alloc()
    axis_z = writer.alloc()
    axis_x = writer.alloc()
    placement = writer.alloc()
    writer.add(origin, "CARTESIAN_POINT('',(0.,0.,0.))")
    writer.add(axis_z, "DIRECTION('',(0.,0.,1.))")
    writer.add(axis_x, "DIRECTION('',(1.,0.,-0.))")
    writer.add(placement, f"AXIS2_PLACEMENT_3D('',#{origin},#{axis_z},#{axis_x})")
    brep = writer.alloc()
    writer.add(
        brep,
        f"ADVANCED_BREP_SHAPE_REPRESENTATION('',(#{placement},#{manifold}),#{geom3d_context_id})",
    )

    product_context = writer.alloc()
    product_definition_context = writer.alloc()
    product = writer.alloc()
    formation = writer.alloc()
    definition = writer.alloc()
    shape = writer.alloc()
    representation = writer.alloc()
    writer.add(product_context, f"PRODUCT_CONTEXT('',#{app_context_id},'mechanical')")
    writer.add(
        product_definition_context,
        f"PRODUCT_DEFINITION_CONTEXT('part definition',#{app_context_id},'design')",
    )
    writer.add(product, f"PRODUCT('{product_name}','{product_name}','',(#" f"{product_context}))")
    writer.add(formation, f"PRODUCT_DEFINITION_FORMATION('','',#{product})")
    writer.add(
        definition,
        f"PRODUCT_DEFINITION('design','',#{formation},#{product_definition_context})",
    )
    writer.add(shape, f"PRODUCT_DEFINITION_SHAPE('','',#{definition})")
    writer.add(representation, f"SHAPE_DEFINITION_REPRESENTATION(#{shape},#{brep})")


def generate_box(path: Path, size_x: float, size_y: float, size_z: float, product_name: str) -> None:
    writer = StepWriter()

    app_protocol = writer.alloc()
    app_context = writer.alloc()
    writer.add(
        app_protocol,
        f"APPLICATION_PROTOCOL_DEFINITION('international standard',\n"
        f"  'automotive_design',2000,#{app_context})",
    )
    writer.add(
        app_context,
        "APPLICATION_CONTEXT(\n"
        "  'core data for automotive mechanical design processes')",
    )

    geom3d = _add_geom3d_context(writer)
    _add_rectangular_box(
        writer,
        size_x / 2,
        size_y / 2,
        size_z / 2,
        geom3d,
        app_context,
        product_name,
    )
    writer.write_to(path)


def main() -> None:
    root = Path(__file__).resolve().parent
    generate_box(root / "support_ladder.step", 1.0, 11.5, 62.5, "VXDLayer0Support")
    generate_box(root / "sensor_ladder.step", 0.05, 11.0, 62.5, "VXDLayer0Sensor")


if __name__ == "__main__":
    main()
