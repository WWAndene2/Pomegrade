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

CurveMethod PolygonMultiplier::BestMethod(int level)
{
    // measured on low-poly spheres, ellipsoids, cylinders and tori (average
    // distance to the true surface): Phong is closest up to level 3 but stops
    // improving (and drifts) beyond; from level 4, circular PN is closest on
    // all four and keeps getting closer with each level
    return level <= 3 ? CurveMethod::Phong : CurveMethod::CircularPN;
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

// Curved PN triangle at barycentric (u, v, w) for corners (a, b, c).
void InterpolatePN(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                   double u, double v, double w, bool circular, double* pos, double* normal)
{
    const double* P[3] = {a.Position, b.Position, c.Position};
    const double* N[3] = {a.Normal, b.Normal, c.Normal};

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
    double b210[3], b120[3], b021[3], b012[3], b102[3], b201[3];
    edgePoint(0, 1, b210); edgePoint(1, 0, b120);
    edgePoint(1, 2, b021); edgePoint(2, 1, b012);
    edgePoint(2, 0, b102); edgePoint(0, 2, b201);

    // centre control point E + (E - V) * k; the paper's k = 1/2. With circular
    // edges, k = 3/4 measured closer to the true surface on a sphere, an ellipsoid,
    // a cylinder and a torus (average and worst case, at x16 and x64)
    const double centreFactor = circular ? 0.75 : 0.5;
    double b111[3];
    for (int k = 0; k < 3; k++)
    {
        double e = (b210[k] + b120[k] + b021[k] + b012[k] + b102[k] + b201[k]) / 6.0;
        double centre = (P[0][k] + P[1][k] + P[2][k]) / 3.0;
        b111[k] = e + (e - centre) * centreFactor;
    }

    // u weights corner a, v corner b, w corner c
    for (int k = 0; k < 3; k++)
    {
        pos[k] = P[0][k]*u*u*u + P[1][k]*v*v*v + P[2][k]*w*w*w
               + 3*b210[k]*u*u*v + 3*b120[k]*u*v*v
               + 3*b201[k]*u*u*w + 3*b021[k]*v*v*w
               + 3*b102[k]*u*w*w + 3*b012[k]*v*w*w
               + 6*b111[k]*u*v*w;
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
    double n110[3], n011[3], n101[3];
    edgeNormal(0, 1, n110); edgeNormal(1, 2, n011); edgeNormal(2, 0, n101);
    for (int k = 0; k < 3; k++)
        normal[k] = N[0][k]*u*u + N[1][k]*v*v + N[2][k]*w*w + n110[k]*u*v + n011[k]*v*w + n101[k]*u*w;
}

}

MultiplierVertex PolygonMultiplier::Interpolate(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                                double u, double v, double w, CurveMethod method)
{
    MultiplierVertex out;

    if (method == CurveMethod::PNTriangles || method == CurveMethod::CircularPN)
    {
        InterpolatePN(a, b, c, u, v, w, method == CurveMethod::CircularPN, out.Position, out.Normal);
        for (int i = 0; i < 3; i++)
            out.Color[i] = u*a.Color[i] + v*b.Color[i] + w*c.Color[i];
        for (int i = 0; i < 2; i++)
            out.TexCoords[i] = u*a.TexCoords[i] + v*b.TexCoords[i] + w*c.TexCoords[i];
        NormalizeNormal(out);
        return out;
    }

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
        out.Position[i] = (1.0 - ShapeFactor) * flat[i] + ShapeFactor * curved;
        out.Normal[i] = u*a.Normal[i] + v*b.Normal[i] + w*c.Normal[i];
        out.Color[i] = u*a.Color[i] + v*b.Color[i] + w*c.Color[i];
    }
    for (int i = 0; i < 2; i++)
        out.TexCoords[i] = u*a.TexCoords[i] + v*b.TexCoords[i] + w*c.TexCoords[i];

    NormalizeNormal(out);
    return out;
}

int PolygonMultiplier::SubdivideTriangle(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
                                         int level, MultiplierVertex (*out)[3], CurveMethod method)
{
    if (level < 1) level = 1;
    if (level > MaxLevel) level = MaxLevel;

    // grid point (i, j): barycentric (1 - (i+j)/level, i/level, j/level)
    MultiplierVertex grid[MaxLevel + 1][MaxLevel + 1];
    for (int i = 0; i <= level; i++)
    {
        for (int j = 0; j <= level - i; j++)
        {
            double v = (double)i / level;
            double w = (double)j / level;
            if (i == 0 && j == 0)               grid[i][j] = a;
            else if (i == level)                grid[i][j] = b;
            else if (j == level)                grid[i][j] = c;
            else                                grid[i][j] = Interpolate(a, b, c, 1.0 - v - w, v, w, method);
        }
    }

    int count = 0;
    for (int i = 0; i < level; i++)
    {
        for (int j = 0; j < level - i; j++)
        {
            out[count][0] = grid[i][j];
            out[count][1] = grid[i+1][j];
            out[count][2] = grid[i][j+1];
            count++;

            if (j < level - i - 1)
            {
                out[count][0] = grid[i+1][j];
                out[count][1] = grid[i+1][j+1];
                out[count][2] = grid[i][j+1];
                count++;
            }
        }
    }
    return count;
}

}
