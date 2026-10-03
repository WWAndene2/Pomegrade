#include "GPU3D_PolygonMultiplier.h"

#include <cmath>

namespace melonDS
{

namespace
{

double Dot(const double* a, const double* b)
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// Projection of point p onto the tangent plane through corner c (normal n).
void ProjectOntoTangentPlane(const double* p, const MultiplierVertex& corner, double* out)
{
    double d[3] = {p[0] - corner.Position[0], p[1] - corner.Position[1], p[2] - corner.Position[2]};
    double dist = Dot(d, corner.Normal);
    for (int i = 0; i < 3; i++)
        out[i] = p[i] - dist * corner.Normal[i];
}

}

double PolygonMultiplier::EdgeFidelity(const MultiplierVertex& a, const MultiplierVertex& b)
{
    double e[3] = {b.Position[0]-a.Position[0], b.Position[1]-a.Position[1], b.Position[2]-a.Position[2]};
    double len = std::sqrt(Dot(e, e));
    if (len < 1e-12)
        return 1;
    // asymmetry: 0 for a circular arc, up to 2
    double asymmetry = std::fabs(Dot(a.Normal, e) + Dot(b.Normal, e)) / len;
    // fade from 0.2 to 0.5: measured to leave low-poly spheres, an ellipsoid,
    // a cylinder and a torus as round as before (torus 0.0034 -> 0.0037 at
    // x16) while a flat face with spherical normals bulges 28% less
    constexpr double from = 0.2, to = 0.5;
    if (asymmetry <= from) return 1;
    if (asymmetry >= to) return 0;
    double x = (asymmetry - from) / (to - from);
    return 1 - x*x*(3 - 2*x);
}

double PolygonMultiplier::PNhongShare(int level)
{
    // Blend with the lowest average distance to the true surface, in steps of
    // 0.05, over six low-poly models: spheres (8x5 and 6x4), an ellipsoid, a
    // cylinder, a torus and an egg. Phong alone is closest at level 2, stops
    // improving beyond and drifts; circular PN keeps improving. Compared to
    // the better of the two alone: 17% closer at levels 3 and 4, 9% at 5,
    // 5% at 6, about equal at 7 and 8.
    static constexpr double share[MaxLevel + 1] = {0, 0, 0, 0.50, 0.75, 0.85, 0.90, 0.95, 0.95};
    if (level < 0) level = 0;
    if (level > MaxLevel) level = MaxLevel;
    return share[level];
}

bool PolygonMultiplier::NormalizeNormal(MultiplierVertex& vertex)
{
    double len = std::sqrt(Dot(vertex.Normal, vertex.Normal));
    if (len < 1e-9)
        return false;
    for (double& n : vertex.Normal)
        n /= len;
    return true;
}

bool PolygonMultiplier::IsCurved(const MultiplierVertex* vertices, int count)
{
    // cos(1 degree): below this, normals are considered identical
    constexpr double SameDirection = 0.99985;
    for (int i = 1; i < count; i++)
    {
        if (Dot(vertices[0].Normal, vertices[i].Normal) < SameDirection)
            return true;
    }
    return false;
}

namespace
{

// The control points of a curved PN triangle: they depend only on the corners,
// so they are computed once per triangle, not at every point placed on it.
struct PNPatch
{
    const double* P[3];
    const double* N[3];
    double b210[3], b120[3], b021[3], b012[3], b102[3], b201[3];
    double b111[3];
    double n110[3], n011[3], n101[3];
};

void SetupPN(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c, bool circular, PNPatch& patch)
{
    const double* const* P = patch.P;
    const double* const* N = patch.N;
    patch.P[0] = a.Position; patch.P[1] = b.Position; patch.P[2] = c.Position;
    patch.N[0] = a.Normal; patch.N[1] = b.Normal; patch.N[2] = c.Normal;

    // edge control point near corner i, towards corner j: (2Pi + Pj - ((Pj-Pi).Ni) Ni) / 3,
    // i.e. Pi + the chord projected on the tangent plane at Pi, divided by 3
    auto edgePoint = [&](int i, int j, double* out) {
        double d[3] = {P[j][0]-P[i][0], P[j][1]-P[i][1], P[j][2]-P[i][2]};
        double wij = Dot(d, N[i]);
        double t[3];
        for (int k = 0; k < 3; k++)
            t[k] = d[k] - wij*N[i][k];

        double scale = 1.0 / 3.0;
        if (circular)
        {
            // circle through Pi and Pj with normals Ni and Nj: angle a between the
            // normals, radius r = chord / (2 sin(a/2)); handle length (4/3) tan(a/4) r
            double cosa = std::fmax(-1.0, std::fmin(1.0, Dot(N[i], N[j])));
            double angle = std::acos(cosa);
            double chord = std::sqrt(Dot(d, d));
            double tlen = std::sqrt(Dot(t, t));
            // up to 120 degrees between normals; beyond, it isn't a smooth surface
            if (angle > 1e-6 && angle < 2.1 && tlen > 1e-12)
            {
                double handle = (4.0 / 3.0) * std::tan(angle / 4.0) * chord / (2.0 * std::sin(angle / 2.0));
                scale = handle / tlen;
            }
        }
        for (int k = 0; k < 3; k++)
            out[k] = P[i][k] + scale * t[k];
    };
    edgePoint(0, 1, patch.b210); edgePoint(1, 0, patch.b120);
    edgePoint(1, 2, patch.b021); edgePoint(2, 1, patch.b012);
    edgePoint(2, 0, patch.b102); edgePoint(0, 2, patch.b201);

    // centre control point E + (E - V) * k; the paper's k = 1/2. With circular
    // edges, k = 3/4 measured closer to the true surface on a sphere, an ellipsoid,
    // a cylinder and a torus (average and worst case, at x16 and x64)
    const double centreFactor = circular ? 0.75 : 0.5;
    for (int k = 0; k < 3; k++)
    {
        double e = (patch.b210[k] + patch.b120[k] + patch.b021[k] + patch.b012[k] + patch.b102[k] + patch.b201[k]) / 6.0;
        double centre = (P[0][k] + P[1][k] + P[2][k]) / 3.0;
        patch.b111[k] = e + (e - centre) * centreFactor;
    }

    // quadratic normals: mid-edge normal reflected across the edge's perpendicular plane
    auto edgeNormal = [&](int i, int j, double* out) {
        double d[3] = {P[j][0]-P[i][0], P[j][1]-P[i][1], P[j][2]-P[i][2]};
        double len2 = Dot(d, d);
        double sum[3] = {N[i][0]+N[j][0], N[i][1]+N[j][1], N[i][2]+N[j][2]};
        double vij = len2 > 0 ? 2.0 * Dot(d, sum) / len2 : 0.0;
        for (int k = 0; k < 3; k++)
            out[k] = sum[k] - vij*d[k];
        double l = std::sqrt(Dot(out, out));
        if (l > 1e-12)
            for (int k = 0; k < 3; k++) out[k] /= l;
    };
    edgeNormal(0, 1, patch.n110); edgeNormal(1, 2, patch.n011); edgeNormal(2, 0, patch.n101);
}

// Curved PN triangle at barycentric (u, v, w): u weights corner a, v corner b, w corner c
void EvaluatePN(const PNPatch& patch, double u, double v, double w, double* pos, double* normal)
{
    const double* const* P = patch.P;
    const double* const* N = patch.N;
    for (int k = 0; k < 3; k++)
    {
        pos[k] = P[0][k]*u*u*u + P[1][k]*v*v*v + P[2][k]*w*w*w
               + 3*patch.b210[k]*u*u*v + 3*patch.b120[k]*u*v*v
               + 3*patch.b201[k]*u*u*w + 3*patch.b021[k]*v*v*w
               + 3*patch.b102[k]*u*w*w + 3*patch.b012[k]*v*w*w
               + 6*patch.b111[k]*u*v*w;
    }
    for (int k = 0; k < 3; k++)
        normal[k] = N[0][k]*u*u + N[1][k]*v*v + N[2][k]*w*w + patch.n110[k]*u*v + patch.n011[k]*v*w + patch.n101[k]*u*w;
}

MultiplierVertex InterpolatePhong(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                  double u, double v, double w)
{
    MultiplierVertex out;
    double flat[3];
    for (int i = 0; i < 3; i++)
        flat[i] = u*a.Position[i] + v*b.Position[i] + w*c.Position[i];

    double pa[3], pb[3], pc[3];
    ProjectOntoTangentPlane(flat, a, pa);
    ProjectOntoTangentPlane(flat, b, pb);
    ProjectOntoTangentPlane(flat, c, pc);

    for (int i = 0; i < 3; i++)
    {
        double curved = u*pa[i] + v*pb[i] + w*pc[i];
        out.Position[i] = (1.0 - PolygonMultiplier::ShapeFactor) * flat[i] + PolygonMultiplier::ShapeFactor * curved;
        out.Normal[i] = u*a.Normal[i] + v*b.Normal[i] + w*c.Normal[i];
        out.Color[i] = u*a.Color[i] + v*b.Color[i] + w*c.Color[i];
    }
    for (int i = 0; i < 2; i++)
        out.TexCoords[i] = u*a.TexCoords[i] + v*b.TexCoords[i] + w*c.TexCoords[i];

    PolygonMultiplier::NormalizeNormal(out);
    return out;
}

MultiplierVertex InterpolateWithPN(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                   const PNPatch& patch, double u, double v, double w)
{
    MultiplierVertex out;
    EvaluatePN(patch, u, v, w, out.Position, out.Normal);
    for (int i = 0; i < 3; i++)
        out.Color[i] = u*a.Color[i] + v*b.Color[i] + w*c.Color[i];
    for (int i = 0; i < 2; i++)
        out.TexCoords[i] = u*a.TexCoords[i] + v*b.TexCoords[i] + w*c.TexCoords[i];
    PolygonMultiplier::NormalizeNormal(out);
    return out;
}

// A point of the triangle; patch is the triangle's PN patch (circular for PNhong),
// unused by Phong
MultiplierVertex InterpolatePrepared(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                     const PNPatch& patch, double u, double v, double w, CurveMethod method, double pnShare)
{
    if (method == CurveMethod::PNhong)
    {
        MultiplierVertex phong = InterpolatePhong(a, b, c, u, v, w);
        if (pnShare <= 0)
            return phong;
        MultiplierVertex pn = InterpolateWithPN(a, b, c, patch, u, v, w);
        for (int i = 0; i < 3; i++)
        {
            phong.Position[i] += pnShare * (pn.Position[i] - phong.Position[i]);
            phong.Normal[i] += pnShare * (pn.Normal[i] - phong.Normal[i]);
        }
        PolygonMultiplier::NormalizeNormal(phong);
        return phong;
    }
    if (method == CurveMethod::PNTriangles || method == CurveMethod::CircularPN)
        return InterpolateWithPN(a, b, c, patch, u, v, w);
    return InterpolatePhong(a, b, c, u, v, w);
}

void SetupPatch(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                CurveMethod method, double pnShare, PNPatch& patch)
{
    if ((method == CurveMethod::PNhong && pnShare > 0) || method == CurveMethod::CircularPN)
        SetupPN(a, b, c, true, patch);
    else if (method == CurveMethod::PNTriangles)
        SetupPN(a, b, c, false, patch);
}

}

MultiplierVertex PolygonMultiplier::Interpolate(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                                double u, double v, double w, CurveMethod method, double pnShare)
{
    PNPatch patch;
    SetupPatch(a, b, c, method, pnShare, patch);
    return InterpolatePrepared(a, b, c, patch, u, v, w, method, pnShare);
}

double PolygonMultiplier::DihedralKeep(double cosAngle)
{
    const double degree = 3.14159265358979323846 / 180.0;
    const double keepUpTo = std::cos(55.0 * degree), flatFrom = std::cos(80.0 * degree);
    if (cosAngle >= keepUpTo) return 1.0;
    if (cosAngle <= flatFrom) return 0.0;
    double x = (cosAngle - flatFrom) / (keepUpTo - flatFrom);
    return x * x * (3 - 2 * x);
}

void PolygonMultiplier::SubdivideGrid(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                      int level, MultiplierVertex* grid, CurveMethod method, const double* edgeKeep)
{
    const double pnShare = PNhongShare(level);
    // geometry fidelity, per edge so both polygons sharing it agree (no cracks)
    double fab = EdgeFidelity(a, b), fbc = EdgeFidelity(b, c), fca = EdgeFidelity(c, a);
    if (edgeKeep)
    {
        fab *= edgeKeep[0];
        fbc *= edgeKeep[1];
        fca *= edgeKeep[2];
    }
    const bool faithful = fab == 1 && fbc == 1 && fca == 1;

    PNPatch patch;
    SetupPatch(a, b, c, method, pnShare, patch);

    // grid point (i, j): barycentric (1 - (i+j)/level, i/level, j/level)
    for (int i = 0; i <= level; i++)
    {
        for (int j = 0; j <= level - i; j++)
        {
            MultiplierVertex& point = grid[GridIndex(i, j, level)];
            double v = (double)i / level;
            double w = (double)j / level;
            if (i == 0 && j == 0)               point = a;
            else if (i == level)                point = b;
            else if (j == level)                point = c;
            else
            {
                double u = 1.0 - v - w;
                point = InterpolatePrepared(a, b, c, patch, u, v, w, method, pnShare);
                if (!faithful)
                {
                    // keep each edge's share of the curvature, blended inside
                    double weight = u*v + v*w + w*u;
                    double keep = (fab*u*v + fbc*v*w + fca*w*u) / weight;
                    for (int k = 0; k < 3; k++)
                    {
                        double flat = u*a.Position[k] + v*b.Position[k] + w*c.Position[k];
                        point.Position[k] = flat + keep * (point.Position[k] - flat);
                    }
                }
            }
        }
    }
}

int PolygonMultiplier::SubdivideTriangle(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                         int level, MultiplierVertex (*out)[3], CurveMethod method,
                                         const double* edgeKeep)
{
    if (level < 1) level = 1;
    if (level > MaxLevel) level = MaxLevel;

    MultiplierVertex grid[MaxGridPoints];
    SubdivideGrid(a, b, c, level, grid, method, edgeKeep);

    int count = 0;
    ForEachGridTriangle(level, [&](int p0, int p1, int p2) {
        out[count][0] = grid[p0];
        out[count][1] = grid[p1];
        out[count][2] = grid[p2];
        count++;
    });
    return count;
}

}
