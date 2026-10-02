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

MultiplierVertex PolygonMultiplier::Interpolate(const MultiplierVertex& a, const MultiplierVertex& b, const MultiplierVertex& c,
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
                                         int level, MultiplierVertex (*out)[3])
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
            else                                grid[i][j] = Interpolate(a, b, c, 1.0 - v - w, v, w);
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
