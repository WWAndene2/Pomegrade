// The remake tooling's rounded zone shapes (SmoothZone) on masks whose answer is known: a rectangle's area, the
// rounding of its corners, its outline's normals, a zone running off the mask's edge, and an empty mask.
#include "TownShapes.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static std::vector<std::vector<bool>> Rect(int rows, int cols, int r0, int r1, int c0, int c1)
{
    std::vector<std::vector<bool>> m(rows, std::vector<bool>(cols, false));
    for (int r = r0; r < r1; r++) for (int c = c0; c < c1; c++) m[r][c] = true;
    return m;
}

static float Area(const ZoneShape& s)
{
    float a = 0;
    for (const auto& t : s.Fill) a += std::fabs((t[1].X - t[0].X) * (t[2].Z - t[0].Z) - (t[1].Z - t[0].Z) * (t[2].X - t[0].X)) / 2;
    return a;
}

int main()
{
    // 10x10 cells of 10 units; a 4x6-cell rectangle inside (240 x 10 x 10 units wide... 40 x 60 = 2400 square units)
    const auto mask = Rect(10, 10, 3, 7, 2, 8);
    const ZoneShape square = SmoothZone(mask, 10, 0, 0, 0);
    // the lattice corners hold the mean of four sub-cells, so even unblurred a corner is cut by one sub-cell (5 units): 12.5 square units each
    check(std::fabs(Area(square) - (2400.0f - 4 * 12.5f)) < 1.0f, "no blur: the rectangle's area less its four corner chamfers");
    check(square.Chains.size() == 1 && square.Chains[0].Closed, "no blur: one closed outline");

    const ZoneShape round = SmoothZone(mask, 10, 0, 0, 1);
    check(Area(round) < 2350.0f && Area(round) > 2400.0f * 0.9f, "blurred: the corners are cut, the area is a little under the rectangle's");
    check(round.Chains.size() == 1 && round.Chains[0].Closed, "blurred: still one closed outline");

    // the corner point of the square mask (20, 30) is cut off: no outline point lies in the rectangle's corner
    float nearest = 1e9f;
    for (const ShapePoint& p : round.Chains[0].Points) nearest = std::min(nearest, std::hypot(p.X - 20.0f, p.Z - 30.0f));
    check(nearest > 2.0f, "blurred: the outline stays away from the rectangle's corner");
    float farthest = 0;
    for (const ShapePoint& p : round.Chains[0].Points) farthest = std::max(farthest, std::hypot(p.X - 50.0f, p.Z - 50.0f));
    check(farthest < std::hypot(30.0f, 20.0f), "blurred: nowhere outside the rectangle's corners");

    // the rectangle is convex: an outward normal has a positive dot product with the vector from its centre (50, 50) to the point
    bool outwardOk = true;
    for (size_t i = 0; i < round.Chains[0].Points.size(); i++)
    {
        const ShapePoint& p = round.Chains[0].Points[i];
        const ShapePoint& n = round.Chains[0].Normals[i];
        if ((p.X - 50.0f) * n.X + (p.Z - 50.0f) * n.Z <= 0) outwardOk = false;
        if (std::fabs(std::hypot(n.X, n.Z) - 1.0f) > 1e-3f) outwardOk = false;
    }
    check(outwardOk, "the outline's normals are unit and point out of the zone");

    // a zone running off the mask's edge: its outline is open, and it still fills to the edge
    const ZoneShape edge = SmoothZone(Rect(10, 10, 3, 7, 5, 10), 10, 0, 0, 1);
    bool open = false;
    for (const ShapeChain& c : edge.Chains) open = open || !c.Closed;
    check(open && Area(edge) > 0.9f * 4 * 5 * 100, "a zone touching the edge: an open outline, filled up to the edge");

    check(SmoothZone(Rect(4, 4, 0, 0, 0, 0), 10, 0, 0, 2).Fill.empty() && SmoothZone({}, 10, 0, 0, 2).Chains.empty(), "an empty mask has no shape");

    // the winding of the fill is the ground's: negative area in (X, Z)
    bool winding = true;
    for (const auto& t : round.Fill) if ((t[1].X - t[0].X) * (t[2].Z - t[0].Z) - (t[1].Z - t[0].Z) * (t[2].X - t[0].X) > 1e-3f) winding = false;
    check(winding, "the fill triangles all face up (the ground's winding)");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
