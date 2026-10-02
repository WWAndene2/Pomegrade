#ifndef GPU3D_POLYGONMULTIPLIER_H
#define GPU3D_POLYGONMULTIPLIER_H

// Polygon multiplier (Pomegrade): subdivides lit 3D polygons into smaller,
// curved ones so low-poly models look rounder. Pure geometry, no GPU state.
//
// Method: Phong tessellation (Boubekeur & Alexa, "Phong Tessellation", 2008).
// Each triangle is split into level*level sub-triangles; every new point is
// pulled towards the tangent planes of the three corners, defined by their
// normals. Corners keep their exact position, and a polygon whose corners all
// share the same normal (a flat surface) stays exactly flat.
//
// Works in view space: positions after the position matrix, normals after the
// vector matrix, as the DS geometry engine computes them for lighting.

namespace melonDS
{

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
    static constexpr int MaxLevel = 4;

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
    static int SubdivideTriangle(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                 int level, MultiplierVertex (*out)[3]);

    // The point at barycentric coordinates (u, v, w) of triangle (a, b, c).
    static MultiplierVertex Interpolate(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                        double u, double v, double w);
};

}

#endif
