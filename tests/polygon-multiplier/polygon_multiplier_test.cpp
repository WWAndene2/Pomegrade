#undef NDEBUG // the checks below are asserts
#include "GPU3D_PolygonMultiplier.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace melonDS;

static const CurveMethod Methods[] = {CurveMethod::Phong, CurveMethod::PNTriangles, CurveMethod::CircularPN, CurveMethod::PNhong};
static const char* MethodNames[] = {"Phong", "PN", "circular PN", "PNhong"};

static MultiplierVertex V(double x, double y, double z, double nx, double ny, double nz)
{
    MultiplierVertex v = {};
    v.Position[0] = x; v.Position[1] = y; v.Position[2] = z;
    v.Normal[0] = nx; v.Normal[1] = ny; v.Normal[2] = nz;
    PolygonMultiplier::NormalizeNormal(v);
    return v;
}

// point of a unit sphere, normal = position
static MultiplierVertex S(double theta, double phi)
{
    double x = std::sin(theta)*std::cos(phi), y = std::cos(theta), z = std::sin(theta)*std::sin(phi);
    return V(x, y, z, x, y, z);
}

static double Cross2D(const MultiplierVertex* t) // z of (t1-t0) x (t2-t0)
{
    return (t[1].Position[0]-t[0].Position[0])*(t[2].Position[1]-t[0].Position[1])
         - (t[1].Position[1]-t[0].Position[1])*(t[2].Position[0]-t[0].Position[0]);
}

static double Radius(const double* p)
{
    return std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
}

static MultiplierVertex Out[PolygonMultiplier::MaxLevel * PolygonMultiplier::MaxLevel][3];

// average distance to the unit sphere of the drawn sub-triangles' centres,
// over one quad of an 8-segment, 5-ring sphere (a typical low-poly DS model)
static double SphereError(int level, CurveMethod method)
{
    const double th0 = 2*M_PI/5, th1 = 3*M_PI/5, ph0 = 0, ph1 = 2*M_PI/8;
    MultiplierVertex q[4] = {S(th0, ph0), S(th1, ph0), S(th1, ph1), S(th0, ph1)};
    double sum = 0; int count = 0;
    for (int t = 0; t < 2; t++)
    {
        int n = PolygonMultiplier::SubdivideTriangle(q[0], q[t+1], q[t+2], level, Out, method);
        for (int i = 0; i < n; i++)
        {
            double c[3];
            for (int k = 0; k < 3; k++)
                c[k] = (Out[i][0].Position[k] + Out[i][1].Position[k] + Out[i][2].Position[k]) / 3;
            sum += std::fabs(Radius(c) - 1);
            count++;
        }
    }
    return sum / count;
}

int main()
{
    for (int m = 0; m < 4; m++)
    {
        CurveMethod method = Methods[m];

        // 1. flat triangle, same normal everywhere: not curved, stays on its plane
        MultiplierVertex f[3] = {V(0,0,5, 0,0,1), V(4,0,5, 0,0,1), V(0,4,5, 0,0,1)};
        assert(!PolygonMultiplier::IsCurved(f, 3));
        int n = PolygonMultiplier::SubdivideTriangle(f[0], f[1], f[2], PolygonMultiplier::MaxLevel, Out, method);
        assert(n == PolygonMultiplier::MaxLevel * PolygonMultiplier::MaxLevel);
        for (int i = 0; i < n; i++)
            for (int k = 0; k < 3; k++)
                assert(std::fabs(Out[i][k].Position[2] - 5) < 1e-9);

        // 2. corners kept exactly, every sub-triangle keeps the winding
        MultiplierVertex t[3] = {V(0,0,0, -1,-1,2), V(3,0,0, 1,-1,2), V(0,3,0, -1,1,2)};
        double parent = Cross2D(t);
        n = PolygonMultiplier::SubdivideTriangle(t[0], t[1], t[2], 3, Out, method);
        assert(n == 9);
        assert(Out[0][0].Position[0] == 0 && Out[0][0].Position[1] == 0);
        for (int i = 0; i < n; i++)
            assert(Cross2D(Out[i]) * parent > 0);

        // 3. two triangles sharing an edge produce the same points on it (no cracks)
        MultiplierVertex q[4] = {V(0,0,0, -1,-1,3), V(2,0,0, 1,-1,3), V(2,2,0.5, 1,1,3), V(0,2,0, -1,1,3)};
        for (int k = 1; k < 8; k++)
        {
            double w = k / 8.0;
            MultiplierVertex e1 = PolygonMultiplier::Interpolate(q[0], q[1], q[2], 1-w, 0, w, method);
            MultiplierVertex e2 = PolygonMultiplier::Interpolate(q[0], q[2], q[3], 1-w, w, 0, method);
            for (int i = 0; i < 3; i++)
                assert(std::fabs(e1.Position[i] - e2.Position[i]) < 1e-12);
        }

        // 4. every method gets a low-poly sphere much closer than the flat polygon
        double flat = SphereError(1, method);
        for (int level = 2; level <= PolygonMultiplier::MaxLevel; level++)
            assert(SphereError(level, method) < flat * 0.5);

        printf("%s: flat stays flat, corners and winding kept, shared edges match, rounder: OK\n", MethodNames[m]);
    }

    // 6. geometry fidelity: a flat cel-shaded face with spherical normals bulges
    //    less, the subdivision stays crack-free, a sphere is untouched
    {
        auto faceBulge = [](bool useFidelity) {
            double worst = 0;
            const int n = 6, level = 4;
            for (int a = 0; a < n; a++)
                for (int b = 0; b < n; b++)
                {
                    MultiplierVertex q[4];
                    double xy[4][2] = {{(double)a, (double)b}, {(double)a, b + 1.0}, {a + 1.0, b + 1.0}, {a + 1.0, (double)b}};
                    for (int k = 0; k < 4; k++)
                    {
                        double x = -0.6 + 1.2 * xy[k][0] / n, y = -0.6 + 1.2 * xy[k][1] / n;
                        q[k] = V(x, y, 1, x, y, 1); // normal away from the head's centre
                    }
                    for (int t = 0; t < 2; t++)
                    {
                        if (useFidelity)
                        {
                            int count = PolygonMultiplier::SubdivideTriangle(q[0], q[t+1], q[t+2], level, Out, CurveMethod::PNhong);
                            for (int i = 0; i < count; i++)
                                for (int k = 0; k < 3; k++)
                                    worst = std::fmax(worst, std::fabs(Out[i][k].Position[2] - 1));
                        }
                        else
                        {
                            for (int i = 0; i <= level; i++)
                                for (int j = 0; j <= level - i; j++)
                                {
                                    MultiplierVertex p = PolygonMultiplier::Interpolate(q[0], q[t+1], q[t+2], 1 - (double)(i + j) / level,
                                        (double)i / level, (double)j / level, CurveMethod::PNhong, PolygonMultiplier::PNhongShare(level));
                                    worst = std::fmax(worst, std::fabs(p.Position[2] - 1));
                                }
                        }
                    }
                }
            return worst;
        };
        double without = faceBulge(false), with = faceBulge(true);
        printf("flat face with spherical normals, bulge at x16: %.4f without fidelity, %.4f with\n", without, with);
        assert(with < without * 0.8);

        // crack-free: the shared edge q0-q2 gets the same points from both triangles
        MultiplierVertex q[4] = {V(0,0,1, 0,0,1), V(0.3,0,1, 0.3,0,1), V(0.3,0.3,1, 0.3,0.3,1), V(0,0.3,1, 0,0.3,1)};
        MultiplierVertex first[64][3];
        int n1 = PolygonMultiplier::SubdivideTriangle(q[0], q[1], q[2], 4, first, CurveMethod::PNhong);
        int n2 = PolygonMultiplier::SubdivideTriangle(q[0], q[2], q[3], 4, Out, CurveMethod::PNhong);
        int shared = 0;
        for (int i = 0; i < n1; i++)
            for (int k = 0; k < 3; k++)
            {
                const double* p = first[i][k].Position;
                // on the diagonal x == y?
                if (std::fabs(p[0] - p[1]) > 1e-9) continue;
                bool found = false;
                for (int j = 0; j < n2 && !found; j++)
                    for (int m = 0; m < 3 && !found; m++)
                        found = std::fabs(Out[j][m].Position[0] - p[0]) < 1e-12 && std::fabs(Out[j][m].Position[1] - p[1]) < 1e-12 &&
                                std::fabs(Out[j][m].Position[2] - p[2]) < 1e-12;
                assert(found);
                shared++;
            }
        assert(shared > 0);

        // a sphere has consistent normals: fidelity changes nothing
        for (int level = 2; level <= PolygonMultiplier::MaxLevel; level++)
        {
            MultiplierVertex s0 = S(1.2, 0.3), s1 = S(1.6, 0.3), s2 = S(1.6, 0.9);
            assert(PolygonMultiplier::EdgeFidelity(s0, s1) == 1 && PolygonMultiplier::EdgeFidelity(s1, s2) == 1 &&
                   PolygonMultiplier::EdgeFidelity(s2, s0) == 1);
        }
        puts("geometry fidelity: less bulge on painted normals, crack-free, spheres untouched: OK");
    }

    // 7. geometry fidelity from the neighbouring polygon: the angle between the
    //    two faces of an edge (a box edge 90 degrees, an 8-sided sphere 45)
    {
        const double degree = M_PI / 180.0;
        assert(PolygonMultiplier::DihedralKeep(std::cos(0.0)) == 1.0);
        assert(PolygonMultiplier::DihedralKeep(std::cos(45 * degree)) == 1.0);
        assert(PolygonMultiplier::DihedralKeep(std::cos(90 * degree)) == 0.0);
        double mid = PolygonMultiplier::DihedralKeep(std::cos(67.5 * degree));
        assert(mid > 0.3 && mid < 0.7);
        for (int a = 0; a < 90; a++)
            assert(PolygonMultiplier::DihedralKeep(std::cos((a + 1) * degree)) <= PolygonMultiplier::DihedralKeep(std::cos(a * degree)));

        // a box face with smoothed normals (the corners' normals point
        // diagonally out of the box): all its edges hard -> it stays flat
        MultiplierVertex a = V(-1, -1, 1, -1, -1, 1), b = V(1, -1, 1, 1, -1, 1), c = V(1, 1, 1, 1, 1, 1);
        const double hard[3] = {0, 0, 0}, soft[3] = {1, 1, 1};
        int n = PolygonMultiplier::SubdivideTriangle(a, b, c, 4, Out, CurveMethod::PNhong, hard);
        double worst = 0;
        for (int i = 0; i < n; i++)
            for (int k = 0; k < 3; k++)
                worst = std::fmax(worst, std::fabs(Out[i][k].Position[2] - 1));
        int nCurved = PolygonMultiplier::SubdivideTriangle(a, b, c, 4, Out, CurveMethod::PNhong, soft);
        double curved = 0;
        for (int i = 0; i < nCurved; i++)
            for (int k = 0; k < 3; k++)
                curved = std::fmax(curved, std::fabs(Out[i][k].Position[2] - 1));
        printf("box face with smoothed normals at x16: bulge %.4f with soft edges, %.4f with hard edges\n", curved, worst);
        assert(curved > 0.05 && worst < 1e-9);
        // all edges kept = the same as without the parameter
        MultiplierVertex ref[PolygonMultiplier::MaxLevel * PolygonMultiplier::MaxLevel][3];
        PolygonMultiplier::SubdivideTriangle(a, b, c, 4, ref, CurveMethod::PNhong);
        for (int i = 0; i < nCurved; i++)
            for (int k = 0; k < 3; k++)
                for (int d = 0; d < 3; d++)
                    assert(ref[i][k].Position[d] == Out[i][k].Position[d]);
        puts("geometry fidelity from neighbours: angle rule, hard edges stay flat, soft unchanged: OK");
    }

    // 5. PNhong, the method the multiplier uses: Phong at level 2, a blend
    //    measured closer than both above (see PNhongShare)
    printf("average distance to the sphere (8-segment model):\n");
    for (int level : {2, 3, 4, 6, 8})
        printf("  x%-3d Phong %.4f  circular PN %.4f  PNhong %.4f (%.0f%% PN)\n", level*level,
               SphereError(level, CurveMethod::Phong), SphereError(level, CurveMethod::CircularPN),
               SphereError(level, CurveMethod::PNhong), PolygonMultiplier::PNhongShare(level) * 100);
    assert(SphereError(2, CurveMethod::PNhong) == SphereError(2, CurveMethod::Phong));
    for (int level = 3; level <= 4; level++)
    {
        double hybrid = SphereError(level, CurveMethod::PNhong);
        assert(hybrid < SphereError(level, CurveMethod::Phong));
        assert(hybrid < SphereError(level, CurveMethod::CircularPN));
    }
    for (int level = 5; level <= PolygonMultiplier::MaxLevel; level++)
        assert(SphereError(level, CurveMethod::PNhong) <= 1.05 * std::fmin(SphereError(level, CurveMethod::Phong),
                                                                              SphereError(level, CurveMethod::CircularPN)));

    puts("ALL OK");
}
