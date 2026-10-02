#undef NDEBUG // the checks below are asserts
#include "GPU3D_PolygonMultiplier.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace melonDS;

static MultiplierVertex V(double x, double y, double z, double nx, double ny, double nz)
{
    MultiplierVertex v = {};
    v.Position[0] = x; v.Position[1] = y; v.Position[2] = z;
    v.Normal[0] = nx; v.Normal[1] = ny; v.Normal[2] = nz;
    PolygonMultiplier::NormalizeNormal(v);
    return v;
}

static double Cross2D(const MultiplierVertex* t) // z of (t1-t0) x (t2-t0)
{
    return (t[1].Position[0]-t[0].Position[0])*(t[2].Position[1]-t[0].Position[1])
         - (t[1].Position[1]-t[0].Position[1])*(t[2].Position[0]-t[0].Position[0]);
}

static double Radius(const MultiplierVertex& v)
{
    return std::sqrt(v.Position[0]*v.Position[0] + v.Position[1]*v.Position[1] + v.Position[2]*v.Position[2]);
}

int main()
{
    MultiplierVertex out[PolygonMultiplier::MaxLevel * PolygonMultiplier::MaxLevel][3];

    // 1. flat triangle, same normal everywhere: not curved, stays on its plane
    MultiplierVertex f[3] = {V(0,0,5, 0,0,1), V(4,0,5, 0,0,1), V(0,4,5, 0,0,1)};
    assert(!PolygonMultiplier::IsCurved(f, 3));
    int n = PolygonMultiplier::SubdivideTriangle(f[0], f[1], f[2], 4, out);
    assert(n == 16);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 3; k++)
            assert(std::fabs(out[i][k].Position[2] - 5) < 1e-9);
    puts("flat polygon stays flat: OK");

    // 2. one face of an octahedron inscribed in the unit sphere, normals = positions:
    //    curved points must be closer to the sphere than flat subdivision
    MultiplierVertex s[3] = {V(1,0,0, 1,0,0), V(0,1,0, 0,1,0), V(0,0,1, 0,0,1)};
    assert(PolygonMultiplier::IsCurved(s, 3));
    for (int level = 2; level <= 4; level++)
    {
        n = PolygonMultiplier::SubdivideTriangle(s[0], s[1], s[2], level, out);
        assert(n == level*level);
        double errCurved = 0, errFlat = 0;
        for (int i = 0; i <= level; i++)
            for (int j = 0; j <= level - i; j++)
            {
                double u = 1.0 - (double)(i+j)/level, v = (double)i/level, w = (double)j/level;
                MultiplierVertex p = PolygonMultiplier::Interpolate(s[0], s[1], s[2], u, v, w);
                double flat = std::sqrt(u*u + v*v + w*w); // |u*e0 + v*e1 + w*e2|
                errCurved = std::fmax(errCurved, std::fabs(Radius(p) - 1));
                errFlat = std::fmax(errFlat, std::fabs(flat - 1));
            }
        printf("level %d: max distance to sphere, flat %.3f -> curved %.3f\n", level, errFlat, errCurved);
        assert(errCurved < errFlat * 0.5);
    }

    // 3. corners kept exactly, every sub-triangle keeps the winding
    MultiplierVertex t[3] = {V(0,0,0, -1,-1,2), V(3,0,0, 1,-1,2), V(0,3,0, -1,1,2)};
    double parent = Cross2D(t);
    n = PolygonMultiplier::SubdivideTriangle(t[0], t[1], t[2], 3, out);
    assert(n == 9);
    assert(out[0][0].Position[0] == 0 && out[0][0].Position[1] == 0);
    for (int i = 0; i < n; i++)
        assert(Cross2D(out[i]) * parent > 0);
    puts("corners kept, winding kept: OK");

    // 4. two triangles sharing an edge produce the same points on it (no cracks)
    MultiplierVertex q[4] = {V(0,0,0, -1,-1,3), V(2,0,0, 1,-1,3), V(2,2,0.5, 1,1,3), V(0,2,0, -1,1,3)};
    for (int k = 1; k < 4; k++)
    {
        double w = k / 4.0;
        // edge q0-q2 seen from triangle (q0,q1,q2) and from triangle (q0,q2,q3)
        MultiplierVertex e1 = PolygonMultiplier::Interpolate(q[0], q[1], q[2], 1-w, 0, w);
        MultiplierVertex e2 = PolygonMultiplier::Interpolate(q[0], q[2], q[3], 1-w, w, 0);
        for (int i = 0; i < 3; i++)
            assert(std::fabs(e1.Position[i] - e2.Position[i]) < 1e-12);
    }
    puts("shared edges match: OK");

    puts("ALL OK");
}
