#ifndef GPU3D_POLYGONMULTIPLIER_H
#define GPU3D_POLYGONMULTIPLIER_H

// Polygon multiplier (Pomegrade): subdivides lit 3D polygons into smaller,
// curved ones so low-poly models look rounder. Pure geometry, no GPU state.
//
// Each triangle is split into level*level sub-triangles placed on a curved
// surface built from the three corners' positions and normals (see
// CurveMethod). Corners keep their exact position, points on an edge depend
// only on that edge's two corners (no cracks between neighbours), and a
// polygon whose corners all share the same normal (a flat surface) stays
// exactly flat.
//
// Works in view space: positions after the position matrix, normals after the
// vector matrix, as the DS geometry engine computes them for lighting.

namespace melonDS
{

enum class CurveMethod
{
    // Phong tessellation (Boubekeur & Alexa 2008): quadratic surface.
    Phong,
    // Curved PN triangles (Vlachos, Peters, Boyd & Mitchell 2001): cubic
    // Bezier surface from the same positions and normals, with quadratic
    // normals. Rounder, and still crack-free along shared edges.
    PNTriangles,
    // PN triangles whose edges follow circular arcs: each edge's tangent
    // handles get the length that makes a cubic Bezier match the circle
    // through its two corners with their normals ((4/3) tan(angle/4) r),
    // instead of a third of the chord, which bulges too little.
    CircularPN,
    // "PNhong": a fixed blend of Phong and circular PN for each level, the
    // blend measured closest to the true surface (see PNhongShare). Phong is
    // closest at low levels, circular PN at high ones, a mix in between. Both
    // put shared-edge points from the edge's corners only, so the blend stays
    // crack-free and exact on flat polygons.
    PNhong,
};

struct MultiplierVertex
{
    double Position[3]; // view space
    double Normal[3];   // view space, unit length
    double Color[3];    // vertex colour, interpolated linearly
    double TexCoords[2];
};

class PolygonMultiplier
{
public:
    // level n = n*n sub-triangles per triangle
    static constexpr int MaxLevel = 8;

    // PNhong: share of circular PN (the rest is Phong) at a level.
    static double PNhongShare(int level);

    // Geometry fidelity: share of the curvature kept along edge (a, b), 1 to 0.
    // On a smooth surface the two corners' normals tilt symmetrically about
    // the edge; normals that tilt one-sidedly don't match the geometry (e.g.
    // spherical normals painted on a flat cel-shaded face, or hand-tuned
    // lighting normals), and curving along them would deform the model.
    static double EdgeFidelity(const MultiplierVertex& a, const MultiplierVertex& b);

    // Geometry fidelity from the neighbouring polygon: share of the curvature
    // kept along an edge, from the cosine of the angle between the two faces
    // that share it. A coarse curved surface has moderate angles (8-sided
    // sphere: 45 degrees), an angular shape sharp ones (box: 90) even when the
    // game smoothed its normals: kept up to 55 degrees, flat from 80.
    static double DihedralKeep(double cosAngle);

    // Phong tessellation shape factor: 0 = flat subdivision, 1 = full
    // projection. 3/4 is the value recommended by the paper.
    static constexpr double ShapeFactor = 0.75;

    // True when the corners' normals differ enough for subdivision to change
    // the shape. Flat polygons (all normals equal) are left alone.
    static bool IsCurved(const MultiplierVertex* vertices, int count);

    // Normalizes the normal in place. Returns false if it has no direction.
    static bool NormalizeNormal(MultiplierVertex& vertex);

    // Splits triangle (a, b, c) into level*level sub-triangles, written to
    // out[i][0..2], with the same winding as (a, b, c). Returns the number of
    // sub-triangles. out must hold MaxLevel*MaxLevel triangles.
    // edgeKeep: optional extra share of the curvature kept along (a, b),
    // (b, c), (c, a), 1 to 0 (see DihedralKeep).
    static int SubdivideTriangle(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                 int level, MultiplierVertex (*out)[3], CurveMethod method,
                                 const double* edgeKeep = nullptr);

    // The point at barycentric coordinates (u, v, w) of triangle (a, b, c).
    // pnShare is only used by PNhong.

    static MultiplierVertex Interpolate(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                        double u, double v, double w, CurveMethod method, double pnShare = 1.0);
};

}

#endif
