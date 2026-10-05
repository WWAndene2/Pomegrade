// The remake tooling's zone shapes (StairZone) on masks whose answer is known: whole cells, square corners, the cut of a
// tip, a zone running off the mask's edge, the outline's normals and the fill's winding.
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
    // ORAS's stair zones (StairZone): whole cells, a point at every lattice point, square corners, tips cut only where asked
    {
        std::vector<std::vector<bool>> one(3, std::vector<bool>(3, false));
        one[1][1] = true;
        const ZoneShape cell = StairZone(one, 10, 0, 0, false);
        check(cell.Fill.size() == 2, "a single cell is two triangles");
        check(cell.Chains.size() == 1 && cell.Chains[0].Closed && cell.Chains[0].Points.size() == 4, "a single cell has one closed chain of four lattice points");
        bool near = true;
        const float lattice[4][2] = {{10, 10}, {20, 10}, {20, 20}, {10, 20}};
        for (const ShapePoint& p : cell.Chains[0].Points)
        {
            bool ok2 = false;
            for (const auto& l : lattice) ok2 = ok2 || (std::fabs(p.X - l[0]) <= 0.91f && std::fabs(p.Z - l[1]) <= 0.91f);
            near = near && ok2;
        }
        check(near, "its points lie within 0.09 of a cell of the lattice points (the jitter)");
        check(StairZone(one, 10, 0, 0, false).Chains[0].Points[0].X == cell.Chains[0].Points[0].X, "the same zone gives the same shape");
        const ZoneShape exact = StairZone(one, 10, 0, 0, false, 0.0f);
        bool onLattice = true;
        for (const ShapePoint& p : exact.Chains[0].Points) onLattice = onLattice && std::fmod(p.X, 10.0f) == 0.0f && std::fmod(p.Z, 10.0f) == 0.0f;
        check(onLattice, "without jitter every point is exactly on a lattice point (the game-wide norm)");

        std::vector<std::vector<bool>> hole(3, std::vector<bool>(3, true));
        hole[1][1] = false;
        const ZoneShape square = StairZone(hole, 10, 0, 0, false), cut = StairZone(hole, 10, 0, 0, true);
        check(square.Chains.size() == 1 && square.Chains[0].Points.size() == 4, "a hole's border is four lattice points");
        // the hole's corner (1, 1) is cut towards the hole by 0.36 of a cell on each axis
        bool arc = false;
        for (const ShapePoint& p : cut.Chains[0].Points) arc = arc || (std::fabs(p.X - 13.6f) < 0.01f && std::fabs(p.Z - 13.6f) < 0.01f);
        check(arc, "with roundTips a corner of the zone that three of its cells meet is cut towards the outside cell");
        bool sharp = true;
        const ZoneShape tipped = StairZone(one, 10, 0, 0, true);
        for (const ShapePoint& p : tipped.Chains[0].Points) sharp = sharp && (p.X < 10.91f && p.X > 9.09f ? true : p.X > 19.09f && p.X < 20.91f);
        check(sharp, "a corner of the zone itself (one cell of four) stays square even with roundTips");

        // a rectangle of 4 x 6 cells: its area is whole cells (jitter aside), outward unit normals, the ground's winding
        const ZoneShape rect = StairZone(Rect(10, 10, 3, 9, 2, 6), 10, 0, 0, false);
        check(std::fabs(Area(rect) - 2400.0f) < 90.0f, "a 4 x 6 cell rectangle fills about 2400 square units (the lattice jitter moves its border by under 0.9)");
        bool outward = rect.Chains.size() == 1 && rect.Chains[0].Closed && rect.Chains[0].Points.size() == 20;
        for (size_t i = 0; outward && i < rect.Chains[0].Points.size(); i++)
        {
            const ShapePoint &p2 = rect.Chains[0].Points[i], &n = rect.Chains[0].Normals[i];
            if ((p2.X - 40.0f) * n.X + (p2.Z - 60.0f) * n.Z <= 0 || std::fabs(std::hypot(n.X, n.Z) - 1.0f) > 1e-3f) outward = false;
        }
        check(outward, "its outline is one closed chain of 20 points whose normals are unit and point out of the zone");
        bool winding = true;
        for (const auto& tri : rect.Fill) if ((tri[1].X - tri[0].X) * (tri[2].Z - tri[0].Z) - (tri[1].Z - tri[0].Z) * (tri[2].X - tri[0].X) > 1e-3f) winding = false;
        check(winding, "the fill triangles all face up (the ground's winding)");
        check(StairZone(Rect(4, 4, 0, 0, 0, 0), 10, 0, 0, true).Fill.empty() && StairZone({}, 10, 0, 0, true).Chains.empty(), "an empty mask has no shape");

        // a zone touching the mask's border runs on: an open chain, not a closed loop
        std::vector<std::vector<bool>> edge(3, std::vector<bool>(3, false));
        edge[0][0] = edge[1][0] = true;
        const ZoneShape run = StairZone(edge, 10, 0, 0, false);
        check(run.Chains.size() == 1 && !run.Chains[0].Closed, "a zone on the border has an open chain");
    }

    // where ORAS's outline strip puts its tips and roots (OutlineStrip): along the normal on a straight run, along the bisector
    // at a corner, at the depths measured on Littleroot (in cells: straight 0.23 in / 0.25 out, the zone's corner 0.32 / 0.22,
    // an inner corner 0.13 / 0.36)
    {
        auto near = [](ShapePoint p, float x, float z) { return std::hypot(p.X - x, p.Z - z) < 1e-3f; };
        // an L: cells (row, col) (1,1) (1,2) (2,1), 10 units a cell; its inner corner is the lattice point (20, 20)
        std::vector<std::vector<bool>> l(4, std::vector<bool>(4, false));
        l[1][1] = l[1][2] = l[2][1] = true;
        const ShapeChain chain = StairZone(l, 10, 0, 0, false, 0).Chains.at(0);
        const OutlinePoints strip = OutlineStrip(chain, 10);
        check(strip.Tips.size() == chain.Points.size() && strip.Roots.size() == chain.Points.size(), "one tip and one root a chain point");
        const float d = 1 / std::sqrt(2.0f);
        bool outer = false, inner = false, straight = false;
        for (size_t i = 0; i < chain.Points.size(); i++)
        {
            const ShapePoint c = chain.Points[i], t = strip.Tips[i], r = strip.Roots[i];
            // the zone's own corner at (10, 10): tip 3.2 towards (+, +), root 2.2 towards (-, -)
            if (near(c, 10, 10)) outer = near(t, 10 + 3.2f * d, 10 + 3.2f * d) && near(r, 10 - 2.2f * d, 10 - 2.2f * d);
            // the inner corner at (20, 20): the outside pokes in from (+, +); tip 1.3 back towards (-, -), root 3.6 out towards (+, +)
            if (near(c, 20, 20)) inner = near(t, 20 - 1.3f * d, 20 - 1.3f * d) && near(r, 20 + 3.6f * d, 20 + 3.6f * d);
            // a straight run's point at (20, 10), on the top side (the zone below it): tip 2.3 down, root 2.5 up
            if (near(c, 20, 10)) straight = near(t, 20, 12.3f) && near(r, 20, 7.5f);
        }
        check(outer, "the zone's corner: tip 0.32 cell in, root 0.22 out, on the bisector");
        check(inner, "an inner corner: tip 0.13 cell in, root 0.36 out, on the bisector");
        check(straight, "a straight run: tip 0.23 cell in, root 0.25 out, on the normal");
    }

    // the zone's own corners pulled in (outerPull), as Littleroot's: a single cell's four corners move 0.12 cell in on the
    // diagonal; an inner corner of an L does not move
    {
        std::vector<std::vector<bool>> one(3, std::vector<bool>(3, false));
        one[1][1] = true;
        const ZoneShape cell = StairZone(one, 10, 0, 0, false, 0, 0.12f);
        const float d = 1.2f / std::sqrt(2.0f);
        bool pulled = cell.Chains.size() == 1 && cell.Chains[0].Points.size() == 4;
        for (const ShapePoint& p : cell.Chains[0].Points)
        {
            const float ex = p.X < 15 ? 10 + d : 20 - d, ez = p.Z < 15 ? 10 + d : 20 - d;
            if (std::hypot(p.X - ex, p.Z - ez) > 1e-3f) pulled = false;
        }
        check(pulled, "a zone's own corners move 0.12 cell into it on the diagonal");
        std::vector<std::vector<bool>> l(4, std::vector<bool>(4, false));
        l[1][1] = l[1][2] = l[2][1] = true;
        bool innerStays = false;
        const ZoneShape lShape = StairZone(l, 10, 0, 0, false, 0, 0.12f);
        for (const ShapePoint& p : lShape.Chains.at(0).Points) if (std::hypot(p.X - 20, p.Z - 20) < 1e-3f) innerStays = true;
        check(innerStays, "an inner corner stays on the lattice");
    }

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
