#include "TownBuilder.h"

#include <set>
#include "BchWriter.h"
#include "BinLinker.h"
#include "TownShapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <functional>

// Twinleaf-style towns from a TownLayout. The donor is Petalburg's map piece (a/0/3/9 piece 8): its terrain
// model's materials (and so its area pack, 9), the house, the flower patch and the hedge are cut out of it by
// the mesh numbers and tile boxes below, which were measured on that piece. The trees are Route 101's (piece 5).
// Another donor needs those numbers measured again.

namespace remake
{

namespace
{

constexpr int N = 40;
constexpr float T = 18;
static float X(float col) { return (col - 20) * T; }
static float Z(float row) { return (row - 20) * T; }

struct Part { std::vector<BchVertex> V; std::vector<uint32_t> I; };

// triangles whose centre lies in a box of tiles, relative to an anchor point (in tiles)
static Part Cut(const BchMesh& m, float c0, float c1, float r0, float r1, float ac, float ar)
{
    Part p; std::map<uint32_t, uint32_t> remap;
    for (size_t t = 0; t + 2 < m.Triangles.size(); t += 3)
    {
        float cx = 0, cz = 0;
        for (int k = 0; k < 3; k++) { cx += m.Vertices[m.Triangles[t + k]].Position[0] / 3; cz += m.Vertices[m.Triangles[t + k]].Position[2] / 3; }
        const float c = cx / T + 20, r = cz / T + 20;
        if (c < c0 || c >= c1 || r < r0 || r >= r1) continue;
        for (int k = 0; k < 3; k++)
        {
            const uint32_t i = m.Triangles[t + k];
            auto it = remap.find(i);
            if (it == remap.end())
            {
                BchVertex v = m.Vertices[i];
                v.Position[0] -= X(ac); v.Position[2] -= Z(ar);
                it = remap.emplace(i, (uint32_t)p.V.size()).first;
                p.V.push_back(v);
            }
            p.I.push_back(it->second);
        }
    }
    return p;
}

// A kit's connected pieces (triangles joined by their vertices' positions) whose centre lies within maxDistance units of the
// anchor (the kit's origin): a rectangle cut out of a piece can catch a sliver of a neighbour, which every copy then carries
// (Route 101's tree cut caught one triangle of the next tree's canopy, 2 tiles off and 27-38 units up: the owner saw it
// float beside the trees).
static Part KeepNear(const Part& p, float maxDistance)
{
    auto key = [&](uint32_t i) { const auto& q = p.V[i].Position; return std::make_tuple(std::lround(q[0] * 10), std::lround(q[1] * 10), std::lround(q[2] * 10)); };
    std::map<std::tuple<long, long, long>, std::tuple<long, long, long>> up;
    std::function<std::tuple<long, long, long>(std::tuple<long, long, long>)> find = [&](std::tuple<long, long, long> k) {
        auto it = up.find(k);
        if (it == up.end() || it->second == k) { up[k] = k; return k; }
        return it->second = find(it->second);
    };
    for (size_t t = 0; t + 2 < p.I.size(); t += 3)
        for (int k = 1; k < 3; k++) up[find(key(p.I[t + k]))] = find(key(p.I[t]));
    std::map<std::tuple<long, long, long>, std::array<float, 3>> centre; // x sum, z sum, count
    for (size_t t = 0; t + 2 < p.I.size(); t += 3)
    {
        auto& c = centre[find(key(p.I[t]))];
        for (int k = 0; k < 3; k++) { c[0] += p.V[p.I[t + k]].Position[0]; c[1] += p.V[p.I[t + k]].Position[2]; c[2] += 1; }
    }
    Part out; std::map<uint32_t, uint32_t> remap;
    for (size_t t = 0; t + 2 < p.I.size(); t += 3)
    {
        const auto& c = centre[find(key(p.I[t]))];
        if (std::hypot(c[0] / c[2], c[1] / c[2]) > maxDistance) continue;
        for (int k = 0; k < 3; k++)
        {
            auto it = remap.find(p.I[t + k]);
            if (it == remap.end()) { it = remap.emplace(p.I[t + k], (uint32_t)out.V.size()).first; out.V.push_back(p.V[p.I[t + k]]); }
            out.I.push_back(it->second);
        }
    }
    return out;
}

// a part placed with its anchor at (c, r) in tiles, scaled about it
static void Place(BchGeometry& g, const Part& p, float c, float r, float scale = 1, bool quarterTurn = false, float heightScale = 1)
{
    const uint32_t base = (uint32_t)g.Vertices.size();
    for (BchVertex v : p.V)
    {
        for (int k = 0; k < 3; k++) v.Position[k] *= scale;
        v.Position[1] *= heightScale;
        if (quarterTurn) { const float x = v.Position[0]; v.Position[0] = -v.Position[2]; v.Position[2] = x; const float nx = v.Normal[0]; v.Normal[0] = -v.Normal[2]; v.Normal[2] = nx; }
        v.Position[0] += X(c); v.Position[2] += Z(r);
        g.Vertices.push_back(v);
    }
    for (uint32_t i : p.I) g.Triangles.push_back(base + i);
}
// flat quads on the tiles of a class, planar texture coordinates (u = x/72, v = -z/72, as the game's ground)
static void Flat(BchGeometry& g, const std::vector<std::string>& vis, const std::string& classes, float y, const float colour[4], int margin = 0)
{
    std::map<std::pair<int, int>, uint32_t> corner;
    auto vertex = [&](int c, int r) {
        auto it = corner.find({c, r});
        if (it != corner.end()) return it->second;
        BchVertex v;
        v.Position[0] = X((float)c); v.Position[1] = y; v.Position[2] = Z((float)r);
        v.TexCoord[0] = v.Position[0] / 72.0f; v.TexCoord[1] = -v.Position[2] / 72.0f;
        std::copy(colour, colour + 4, v.Colour);
        const uint32_t i = (uint32_t)g.Vertices.size();
        g.Vertices.push_back(v);
        return corner[{c, r}] = i;
    };
    for (int r = -margin; r < N + margin; r++)
        for (int c = -margin; c < N + margin; c++)
        {
            const char ch = vis[std::clamp(r, 0, N - 1)][std::clamp(c, 0, N - 1)];
            if (classes.find(ch) == std::string::npos) continue;
            const uint32_t a = vertex(c, r), b = vertex(c + 1, r), d = vertex(c + 1, r + 1), e = vertex(c, r + 1);
            for (uint32_t i : {a, e, d, a, d, b}) g.Triangles.push_back(i);
        }
}

// flat quads on the cells (1/k tile each) a predicate selects, planar texture coordinates as the game's ground
template <typename F>
static void FlatFine(BchGeometry& g, int k, F select, float y, const float colour[4])
{
    std::map<std::pair<int, int>, uint32_t> corner;
    const float cell = T / k;
    auto vertex = [&](int c, int r) {
        auto it = corner.find({c, r});
        if (it != corner.end()) return it->second;
        BchVertex v;
        v.Position[0] = c * cell - 360; v.Position[1] = y; v.Position[2] = r * cell - 360;
        v.TexCoord[0] = v.Position[0] / 72.0f; v.TexCoord[1] = -v.Position[2] / 72.0f;
        std::copy(colour, colour + 4, v.Colour);
        const uint32_t i = (uint32_t)g.Vertices.size();
        g.Vertices.push_back(v);
        return corner[{c, r}] = i;
    };
    auto quad = [&](int c, int r, int size) {
        const uint32_t a = vertex(c, r), b = vertex(c + size, r), d = vertex(c + size, r + size), e = vertex(c, r + size);
        for (uint32_t i : {a, e, d, a, d, b}) g.Triangles.push_back(i);
    };
    // a whole tile in one quad when all its cells are selected (the plane is flat: no crack at the joins)
    for (int tr = 0; tr < N; tr++)
        for (int tc = 0; tc < N; tc++)
        {
            bool all = true;
            for (int r = 0; r < k; r++) for (int c = 0; c < k; c++) all = all && select(tc * k + c, tr * k + r);
            if (all) { quad(tc * k, tr * k, k); continue; }
            for (int r = 0; r < k; r++) for (int c = 0; c < k; c++) if (select(tc * k + c, tr * k + r)) quad(tc * k + c, tr * k + r, 1);
        }
}

// the pond's banks: a wall from the ground down past the water on every edge between water and land, on the
// half-tile water grid, textured as Petalburg's (u along the bank / 36, v = 0.843 - y / 100)
// v0, v1: the wall's texture rows at its top (the ground) and at its foot, 9 units down (gake_01_touka: 0.843 to 0.933)
static void BanksFine(BchGeometry& g, const std::vector<std::string>& water2, float v0 = 0.843f, float v1 = 0.933f)
{
    const int M = 2 * N; const float cell = T / 2;
    auto water = [&](int c, int r) { return c >= 0 && r >= 0 && c < M && r < M && water2[r][c] == '~'; };
    auto x = [&](int c) { return c * cell - 360; };
    auto wall = [&](float x0, float z0, float x1, float z1) {
        const uint32_t base = (uint32_t)g.Vertices.size();
        const float ys[2] = {0.0f, -9.0f}, len = std::hypot(x1 - x0, z1 - z0);
        for (float y : ys) for (int e = 0; e < 2; e++)
        {
            BchVertex v; v.Position[0] = e ? x1 : x0; v.Position[1] = y; v.Position[2] = e ? z1 : z0;
            v.TexCoord[0] = (e ? len : 0) / 36.0f; v.TexCoord[1] = v0 + (v1 - v0) * (-y / 9.0f); g.Vertices.push_back(v);
        }
        for (uint32_t i : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(base + i);
    };
    for (int r = 0; r < M; r++) for (int c = 0; c < M; c++)
    {
        if (!water(c, r)) continue;
        if (r > 0 && !water(c, r - 1)) wall(x(c + 1), x(r), x(c), x(r));
        if (r < M - 1 && !water(c, r + 1)) wall(x(c), x(r + 1), x(c + 1), x(r + 1));
        if (c > 0 && !water(c - 1, r)) wall(x(c), x(r), x(c), x(r + 1));
        if (c < M - 1 && !water(c + 1, r)) wall(x(c + 1), x(r + 1), x(c + 1), x(r));
    }
}


// A zone's rounded fill (TownShapes) as ground triangles at a height, textured by the plane as the game's ground
// (u = x/72, v = -z/72). Corners shared by triangles are one vertex.
static void AddFill(BchGeometry& g, const ZoneShape& shape, float y, const float colour[4])
{
    std::map<std::pair<int, int>, uint32_t> corner;
    auto vertex = [&](const ShapePoint& p) {
        const std::pair<int, int> key{(int)std::lround(p.X * 100), (int)std::lround(p.Z * 100)};
        auto it = corner.find(key);
        if (it != corner.end()) return it->second;
        BchVertex v;
        v.Position[0] = p.X; v.Position[1] = y; v.Position[2] = p.Z;
        v.TexCoord[0] = p.X / 72.0f; v.TexCoord[1] = -p.Z / 72.0f;
        std::copy(colour, colour + 4, v.Colour);
        const uint32_t i = (uint32_t)g.Vertices.size();
        g.Vertices.push_back(v);
        return corner[key] = i;
    };
    for (const auto& t : shape.Fill) for (const ShapePoint& p : t) g.Triangles.push_back(vertex(p));
}

// The grass's soft edge around a zone (its outline), laid on the zone's border with its tips and roots where ORAS puts them at
// each lattice point (OutlineStrip, measured: about 4 units into the zone and 4.5 out on a straight run), 0.3 above the ground,
// with Littleroot's chip_grass_decolate mesh's height and texture band (measured). The blades are chip_alpha's grass band (rows 38-60 of 128): the tips (v 0.302, white) into the zone, the roots
// (v 0.496, the grass's colour) out on the grass, so the grass's edge is jagged and runs on beyond it. u advances 0.0245 a
// unit along the border, wrapping inside the band (u 0.046-0.453), the strip cut where it wraps.
static void AddOutline(BchGeometry& g, const ShapeChain& chain, const float tip[4], const float root[4])
{
    const float perUnit = 0.0245f, lo = 0.046f, span = 0.407f;
    const OutlinePoints strip = OutlineStrip(chain, T);
    float s = 0;
    const size_t n = chain.Points.size();
    const size_t last = chain.Closed ? n : n - 1;
    auto vertex = [&](const ShapePoint& p, bool tipSide, float u) {
        BchVertex v;
        v.Position[0] = p.X; v.Position[1] = 0.3f; v.Position[2] = p.Z;
        v.TexCoord[0] = u; v.TexCoord[1] = tipSide ? 0.302f : 0.496f;
        std::copy(tipSide ? tip : root, (tipSide ? tip : root) + 4, v.Colour);
        g.Vertices.push_back(v);
    };
    // a quad starts where the one before it ended (same points, same u) unless the texture band wrapped: its first two
    // vertices are then the previous quad's last two, shared. The tall grass of Sinnoh's routes took the mesh past the 65,536
    // vertices 16-bit indices reach (s1: 81,776 on piece (6, 14)) with four vertices a quad
    bool joined = false;
    uint32_t endTip = 0, endRoot = 0;
    ShapePoint endTipAt{}, endRootAt{};
    float endU = -1;
    for (size_t i = 0; i < last; i++)
    {
        const size_t j = (i + 1) % n;
        const ShapePoint &a = chain.Points[i], &b = chain.Points[j], &na = chain.Normals[i], &nb = chain.Normals[j];
        const float length = std::hypot(b.X - a.X, b.Z - a.Z);
        if (length < 1e-4f) continue;
        float at = 0; // along this segment
        while (at < length - 1e-5f)
        {
            // up to where the texture band wraps or the segment ends
            const float u0 = lo + std::fmod((s + at) * perUnit, span);
            const float toWrap = (lo + span - u0) / perUnit;
            const float to = std::min(length, at + std::max(toWrap, 1e-3f));
            auto lerp = [&](float x, float y2, float t) { return x + (y2 - x) * t; };
            // the tips' and the roots' lines between this point's and the next one's
            auto along = [&](float d, ShapePoint& p, ShapePoint& nrm, ShapePoint& tipAt, ShapePoint& rootAt) {
                const float t = d / length;
                p = {lerp(a.X, b.X, t), lerp(a.Z, b.Z, t)};
                nrm = {lerp(na.X, nb.X, t), lerp(na.Z, nb.Z, t)};
                tipAt = {lerp(strip.Tips[i].X, strip.Tips[j].X, t), lerp(strip.Tips[i].Z, strip.Tips[j].Z, t)};
                rootAt = {lerp(strip.Roots[i].X, strip.Roots[j].X, t), lerp(strip.Roots[i].Z, strip.Roots[j].Z, t)};
            };
            ShapePoint p0, n0, t0, r0, p1, n1, t1, r1;
            along(at, p0, n0, t0, r0); along(to, p1, n1, t1, r1);
            const float u1 = u0 + (to - at) * perUnit;
            uint32_t q[4]; // tip, root at the start; tip, root at the end
            // (within rounding: u0 comes from fmod, the points from interpolations that meet at the same place)
            auto near = [](const ShapePoint& x, const ShapePoint& y) { return std::fabs(x.X - y.X) < 1e-3f && std::fabs(x.Z - y.Z) < 1e-3f; };
            if (joined && std::fabs(u0 - endU) < 1e-5f && near(t0, endTipAt) && near(r0, endRootAt))
            { q[0] = endTip; q[1] = endRoot; }
            else { q[0] = (uint32_t)g.Vertices.size(); vertex(t0, true, u0); q[1] = (uint32_t)g.Vertices.size(); vertex(r0, false, u0); }
            q[2] = (uint32_t)g.Vertices.size(); vertex(t1, true, u1); q[3] = (uint32_t)g.Vertices.size(); vertex(r1, false, u1);
            joined = true; endTip = q[2]; endRoot = q[3]; endTipAt = t1; endRootAt = r1; endU = u1;
            // facing up, as the ground (see the quads': a, e, d / a, d, b); the zone is on the tips' side
            const float cross = (p1.X - p0.X) * n0.Z - (p1.Z - p0.Z) * n0.X;
            if (cross > 0) for (uint32_t k : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(q[k]);
            else for (uint32_t k : {0u, 3u, 2u, 0u, 1u, 3u}) g.Triangles.push_back(q[k]);
            at = to;
        }
        s += length;
    }
}

// A white picket fence as ORAS builds one (piece 153's c103_saku mesh, measured): vertical panels 14 units high standing on the
// ground, the texture (two pickets) once a panel, panels about 24 units long (23.1 and 24.6 measured), each made of two sheets
// 0.25 apart facing out of either side, the front one's vertex colour white, the back one's 0.78. Here a panel run follows each
// straight row or column of fence tiles, from the first tile's centre to the last's; a lone fence tile (no fence beside it) has
// no run and so no panel (Twinleaf's fences are closed rectangles: none is lone).
static void AddFence(BchGeometry& g, const std::vector<std::string>& vis, int N, float corner)
{
    auto fence = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && vis[r][c] == 'F'; };
    auto run = [&](float x0, float z0, float x1, float z1) {
        const float length = std::hypot(x1 - x0, z1 - z0);
        if (length < 1e-3f) return;
        const int panels = std::max(1, (int)std::lround(length / 24));
        const float ux = (x1 - x0) / length, uz = (z1 - z0) / length;
        const float nx = -uz, nz = ux; // the side a sheet faces: along x up = n (the ground's convention, TownBuilder's Flat)
        for (int k = 0; k < panels; k++)
            for (int side : {1, -1})
            {
                const float ax = x0 + (x1 - x0) * k / panels, az = z0 + (z1 - z0) * k / panels;
                const float bx = x0 + (x1 - x0) * (k + 1) / panels, bz = z0 + (z1 - z0) * (k + 1) / panels;
                const float ox = nx * 0.125f * side, oz = nz * 0.125f * side, shade = side > 0 ? 1.0f : 0.78f;
                const uint32_t base = (uint32_t)g.Vertices.size();
                const float corners[4][4] = {{ax, 0, az, 0}, {ax, 14, az, 0}, {bx, 0, bz, 1}, {bx, 14, bz, 1}}; // x, y, z, u
                for (const auto& p : corners)
                {
                    BchVertex v;
                    v.Position[0] = p[0] + ox; v.Position[1] = p[1]; v.Position[2] = p[2] + oz;
                    v.TexCoord[0] = p[3]; v.TexCoord[1] = p[1] / 14;
                    v.Colour[0] = v.Colour[1] = v.Colour[2] = shade; v.Colour[3] = 1;
                    g.Vertices.push_back(v);
                }
                if (side > 0) for (uint32_t i : {0u, 2u, 1u, 1u, 2u, 3u}) g.Triangles.push_back(base + i);
                else for (uint32_t i : {0u, 1u, 2u, 1u, 3u, 2u}) g.Triangles.push_back(base + i);
            }
    };
    auto centre = [&](int i) { return corner + (i + 0.5f) * T; };
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (fence(c, r) && !fence(c - 1, r) && fence(c + 1, r)) { int e = c; while (fence(e + 1, r)) e++; run(centre(c), centre(r), centre(e), centre(r)); }
            if (fence(c, r) && !fence(c, r - 1) && fence(c, r + 1)) { int e = r; while (fence(c, e + 1)) e++; run(centre(c), centre(r), centre(c), centre(e)); }
        }
}

// The snow's edge made of clumps (the owner's choice): the earth-patch decal of Littleroot's chip_alpha (its cluster of stones,
// u 0.02-0.48, v 0.52-0.98, the quad GrassDecals lays) in a texture filled with the ice cave's snow (OrasTown's snow_clump),
// laid along the snow zone's border, centred on it so half of each clump lies on the grass: one every 12 units of border,
// 20 to 28 units wide, turned at random (a hash of its place: the same edge for the same town), 0.5 up, over the snow (0.45).
// open(x, z): whether the ground there is open (not a tree, the forest or a house); a clump is laid only where the ground both
// at its centre and half a tile out from the snow is open, so none lies half under a trunk (the owner: they clipped the trees).
template <typename Open>
static void SnowClumps(BchGeometry& g, const ShapeChain& chain, Open open)
{
    const size_t n = chain.Points.size();
    const size_t last = chain.Closed ? n : n - 1;
    float next = 0, s = 0;
    for (size_t i = 0; i < last; i++)
    {
        const ShapePoint a = chain.Points[i], b = chain.Points[(i + 1) % n];
        const float length = std::hypot(b.X - a.X, b.Z - a.Z);
        for (; next <= s + length; next += 12)
        {
            const float t = length > 1e-4f ? (next - s) / length : 0;
            const float cx = a.X + (b.X - a.X) * t, cz = a.Z + (b.Z - a.Z) * t;
            const ShapePoint na = chain.Normals[i], nb = chain.Normals[(i + 1) % n];
            const float nx = na.X + (nb.X - na.X) * t, nz = na.Z + (nb.Z - na.Z) * t;
            if (!open(cx, cz) || !open(cx + nx * T / 2, cz + nz * T / 2)) continue;
            uint32_t h = (uint32_t)std::lround(cx * 7) * 73856093u ^ (uint32_t)std::lround(cz * 7) * 19349663u;
            h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
            const float size = 20.0f + (float)(h % 9), angle = (float)((h >> 8) % 628) / 100.0f;
            const float ca = std::cos(angle), sa = std::sin(angle);
            const uint32_t base = (uint32_t)g.Vertices.size();
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++)
                {
                    const float lx = (k - 0.5f) * size, lz = (j - 0.5f) * size;
                    BchVertex v;
                    v.Position[0] = cx + lx * ca - lz * sa; v.Position[1] = 0.5f; v.Position[2] = cz + lx * sa + lz * ca;
                    v.TexCoord[0] = k ? 0.48f : 0.02f; v.TexCoord[1] = j ? 0.52f : 0.98f;
                    for (int c = 0; c < 4; c++) v.Colour[c] = 1.0f;
                    g.Vertices.push_back(v);
                }
            for (uint32_t k : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(base + k);
        }
        s += length;
    }
}

// The rim of the playable ground, as Littleroot lays it (its chip_edge_tex mesh, measured): a strip along the border where
// walkable ground meets the solid trees and forest around it (the border of the rounded walkable zone), 9 units wide on
// the solid side, rising from 1 at the border to 3.5 outside, in the rim's blue-green. The texture is projected by the
// material (chip_grass_edge, from the stored positions), so the coordinates are Littleroot's own.
// covered(x, z): whether the walkable ground there lies under snow; a segment whose inner side (half a tile in) is snow is left
// out, so the rim does not show as a grey line over the snow against the trees.
template <typename Covered>
static void AddRim(BchGeometry& g, const ShapeChain& chain, Covered covered)
{
    const float colour[4] = {0.26f, 0.75f, 0.87f, 1.0f};
    const size_t n = chain.Points.size();
    const size_t last = chain.Closed ? n : n - 1;
    for (size_t i = 0; i < last; i++)
    {
        const ShapePoint &a = chain.Points[i], &b = chain.Points[(i + 1) % n], &na = chain.Normals[i], &nb = chain.Normals[(i + 1) % n];
        if (std::hypot(b.X - a.X, b.Z - a.Z) < 1e-4f) continue;
        if (covered((a.X + b.X) / 2 - (na.X + nb.X) / 2 * T / 4, (a.Z + b.Z) / 2 - (na.Z + nb.Z) / 2 * T / 4)) continue;
        const uint32_t base = (uint32_t)g.Vertices.size();
        const ShapePoint ends[2] = {a, b}, normals[2] = {na, nb};
        for (int e = 0; e < 2; e++)
            for (int side = 0; side < 2; side++)
            {
                BchVertex v;
                v.Position[0] = ends[e].X + normals[e].X * 9.0f * side; v.Position[1] = side ? 3.5f : 1.0f; v.Position[2] = ends[e].Z + normals[e].Z * 9.0f * side;
                v.TexCoord[0] = e ? -1.0f : -1.5f; v.TexCoord[1] = side ? 1.0f : 0.75f;
                std::copy(colour, colour + 4, v.Colour);
                g.Vertices.push_back(v);
            }
        const float cross = (b.X - a.X) * na.Z - (b.Z - a.Z) * na.X;
        if (cross > 0) for (uint32_t k : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(base + k);
        else for (uint32_t k : {0u, 3u, 2u, 0u, 1u, 3u}) g.Triangles.push_back(base + k);
    }
}

}

// Loose decals on the open grass, as Littleroot scatters them (square quads of chip_alpha, 0.2 above the ground): a patch
// of bare earth (texture u 0.02-0.48, v 0.52-0.98; 35 units) or a drift of flowers (u 0.51-0.99, v 0.51-0.99; 34-66 units,
// turned at random). About one per 60 tiles of plain open grass (Littleroot: 13 over some 1600 tiles, here denser because
// the town's open ground is small), on a tile whose 3x3 neighbourhood is plain, never within 4 tiles of another, taken in
// the order of a fixed hash of the tile: the same layout for the same window.
template <typename Plain>
static int GrassDecals(BchGeometry& g, Plain plain)
{
    struct Candidate { uint32_t Hash; int C, R; };
    std::vector<Candidate> candidates;
    int plainTiles = 0;
    for (int r = 1; r < N - 1; r++)
        for (int c = 1; c < N - 1; c++)
        {
            if (!plain(c, r)) continue;
            plainTiles++;
            bool around = true;
            for (int dr = -1; dr <= 1 && around; dr++) for (int dc = -1; dc <= 1; dc++) if (!plain(c + dc, r + dr)) { around = false; break; }
            if (!around) continue;
            uint32_t h = (uint32_t)(c * 73856093) ^ (uint32_t)(r * 19349663);
            h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
            candidates.push_back({h, c, r});
        }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.Hash != b.Hash ? a.Hash < b.Hash : a.R != b.R ? a.R < b.R : a.C < b.C; });
    const int wanted = std::max(1, plainTiles / 60);
    std::vector<std::pair<int, int>> placed;
    for (const Candidate& cand : candidates)
    {
        if ((int)placed.size() >= wanted) break;
        bool clear = true;
        for (const auto& [pc, pr] : placed) if (std::abs(pc - cand.C) < 4 && std::abs(pr - cand.R) < 4) clear = false;
        if (!clear) continue;
        placed.push_back({cand.C, cand.R});
        const uint32_t h = cand.Hash;
        const bool earth = (h >> 8) % 4 == 0;
        const float size = earth ? 35.0f : 34.0f + (float)((h >> 12) % 33), angle = earth ? 0.0f : (float)((h >> 4) % 628) / 100.0f;
        const float u0 = earth ? 0.02f : 0.51f, u1 = earth ? 0.48f : 0.99f, v0 = earth ? 0.52f : 0.51f, v1 = earth ? 0.98f : 0.99f;
        const float cx = X(cand.C + 0.5f), cz = Z(cand.R + 0.5f), ca = std::cos(angle), sa = std::sin(angle);
        const uint32_t base = (uint32_t)g.Vertices.size();
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
            {
                const float lx = (i - 0.5f) * size, lz = (j - 0.5f) * size;
                BchVertex v;
                v.Position[0] = cx + lx * ca - lz * sa; v.Position[1] = 0.2f; v.Position[2] = cz + lx * sa + lz * ca;
                v.TexCoord[0] = i ? u1 : u0; v.TexCoord[1] = j ? v0 : v1; // z up the page: v down
                for (int k = 0; k < 4; k++) v.Colour[k] = 1.0f;
                g.Vertices.push_back(v);
            }
        // facing up, as the ground's quads (a, e, d / a, d, b)
        for (uint32_t i : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(base + i);
    }
    return (int)placed.size();
}

Bytes BuildTown(const TownLayout& layout, const TownSources& src, std::vector<std::string>* log)
{

    std::vector<std::string> coll = layout.Collision;
    const std::vector<std::string>& vis = layout.Vis;
    std::vector<std::array<int, 3>> doors;
    for (const TownDoor& d : layout.Doors) doors.push_back({d.Column, d.Row, 0});
    const Bytes& lr = src.Target; const Bytes& petal = src.Donor; const Bytes& r101 = src.Trees;
    auto note = [&](const char* format, auto... args) { char line[256]; snprintf(line, sizeof line, format, args...); if (log) log->push_back(line); };
    const Bytes petalTerrain = BinLinker::Read(petal, "GR").Files[1];
    const BchModel pm = Bch::Read(petalTerrain).Models[0];
    const BchModel rm = Bch::Read(BinLinker::Read(r101, "GR").Files[1]).Models[0];
    // Petalburg's meshes (world02_02_03)
    enum { FlowerA = 0, FlowerB = 1, Canopy = 2, Bank = 11, Ground = 12, Soil = 13, Pale = 14, House = 16, Frame = 17, Window = 18,
           Hedge = 7, Trunk = 19, Outline = 20, Edge = 21, SnowBand = 22, Wall = 24, Water = 25, Shadow = 26 };

    // kits: Petalburg's house at (24-28, 21-25), its door (25, 25) the anchor; a flower patch; Route 101's tree
    std::map<int, Part> house;
    for (int m : {House, Frame, Window, Wall}) house[m] = Cut(pm.Meshes[m], 23.5f, 29.5f, 21.4f, 26.7f, 25.5f, 25.5f);
    std::map<int, Part> flowers = {{FlowerA, Cut(pm.Meshes[FlowerA], 21.0f, 22.2f, 24.8f, 25.9f, 21.5f, 25.4f)},
                                   {FlowerB, Cut(pm.Meshes[FlowerB], 21.0f, 22.2f, 24.8f, 25.9f, 21.5f, 25.4f)}};
    std::map<int, Part> tree = {{Canopy, KeepNear(Cut(rm.Meshes[0], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f), T)},
                                {Trunk, KeepNear(Cut(rm.Meshes[5], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f), T)},
                                {Shadow, KeepNear(Cut(rm.Meshes[9], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f), T)}};
    for (auto& [m, p] : house) note("house kit mesh %d: %zu triangles\n", m, p.I.size() / 3);

    std::map<size_t, BchGeometry> geo;
    for (size_t m = 0; m < pm.Meshes.size(); m++) geo[m].Mesh = m;
    int nLedges = 0;
    if (src.Ledges)
    {
        if (!src.SnowClumpTexture.empty()) throw FormatError("ledges and snow clumps both need the snow-band mesh");
        Part ledge = Cut(rm.Meshes.at(7), 19.0f, 20.0f, 27.5f, 29.5f, 19.5f, 28.5f);
        // in the snow-band mesh the texture showed upside down (the owner, run ledge2: its grass under the earth), with Route 101's
        // own coordinates (grass at v 0.93 on the ledge's top, earth at 0.47 at its foot): the two materials map v the opposite
        // way (their texture transforms are not read: inferred), so v is turned over
        for (BchVertex& v : ledge.V) v.TexCoord[1] = 1.0f - v.TexCoord[1];
        if (ledge.I.empty()) throw FormatError("Route 101's ledge mesh has no triangle at tile 19 of row 28");
        for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
            if (vis[r][c] == 'L') { Place(geo[SnowBand], ledge, c + 0.5f, r + 0.5f); coll[r][c] = 'v'; nLedges++; }
        note("%d ledge tiles (%zu triangles each)\n", nLedges, ledge.I.size() / 3);
    }
    // ground: Petalburg's grass colour, darker under the forest; paths and pale patches on their own meshes
    float grass[4] = {0, 0, 0, 0}, soil[4] = {0, 0, 0, 0}, waterColour[4];
    for (const BchVertex& v : pm.Meshes[Ground].Vertices) for (int k = 0; k < 4; k++) grass[k] += v.Colour[k] / pm.Meshes[Ground].Vertices.size();
    for (const BchVertex& v : pm.Meshes[Soil].Vertices) for (int k = 0; k < 4; k++) soil[k] += v.Colour[k] / pm.Meshes[Soil].Vertices.size();
    std::copy(pm.Meshes[Water].Vertices[0].Colour, pm.Meshes[Water].Vertices[0].Colour + 4, waterColour);
    // colours a step toward Platinum's: the shown colour (texture x vertex colour) moved 35% of the way to the
    // Platinum texture's mean, the vertex colour then that over the texture (at most 1). Means measured on
    // area pack 9 and on Platinum's textures (RGB 0-255)
    auto toward = [](float vc[4], const float tex[3], const float plat[3]) {
        for (int k = 0; k < 3; k++)
        {
            const float shown = tex[k] * vc[k], target = shown + 0.35f * (plat[k] - shown);
            vc[k] = std::clamp(target / tex[k], 0.0f, 1.0f);
        }
    };
    const float kusaTex[3] = {25, 173, 82}, ngrass[3] = {85, 253, 150};
    const float soilTex[3] = {209, 174, 100}, nsand[3] = {255, 231, 157};
    const float riverTex[3] = {126, 200, 195}, puddle[3] = {156, 222, 255};
    const float woodTex[3] = {36, 134, 79}, tree01[3] = {98, 118, 55};
    note("colours before: grass %.2f %.2f %.2f, path %.2f %.2f %.2f, water %.2f %.2f %.2f\n", grass[0], grass[1], grass[2], soil[0], soil[1], soil[2], waterColour[0], waterColour[1], waterColour[2]);
    toward(grass, kusaTex, ngrass); toward(soil, soilTex, nsand); toward(waterColour, riverTex, puddle);
    note("colours after:  grass %.2f %.2f %.2f, path %.2f %.2f %.2f, water %.2f %.2f %.2f\n", grass[0], grass[1], grass[2], soil[0], soil[1], soil[2], waterColour[0], waterColour[1], waterColour[2]);
    for (BchVertex& v : tree[Canopy].V) toward(v.Colour, woodTex, tree01);
    float white[4] = {1, 1, 1, 1};
    // the target's own grass: the mean vertex colour of its two grass meshes (as its artists painted them)
    bool ownGrass = false, haveBlades = false;
    float bladeTip[4] = {}, bladeRoot[4] = {};
    if (src.TargetGrass)
    {
        const BchModel tm = Bch::Read(BinLinker::Read(lr, "GR").Files[1]).Models[0];
        bool haveA = false, haveB = false;
        float colourA[4] = {}, colourB[4] = {};
        for (const BchMesh& m : tm.Meshes)
        {
            const std::string& t = tm.Materials[m.Material].Texture[0];
            // its outline's blades: the tips' and the roots' colours as its artists painted them (Littleroot: tips white, so
            // they take the lighter grass's colour; roots 0.66 0.93 0.80, the grass's), so they fade into both sides
            if (t == "chip_alpha")
            {
                float tips[4] = {}, roots[4] = {};
                int nTips = 0, nRoots = 0;
                for (const BchVertex& v : m.Vertices)
                {
                    const bool tip = std::fabs(v.TexCoord[1] - 0.302f) < 0.01f, root = std::fabs(v.TexCoord[1] - 0.496f) < 0.01f;
                    for (int k = 0; k < 4; k++) { if (tip) tips[k] += v.Colour[k]; if (root) roots[k] += v.Colour[k]; }
                    nTips += tip; nRoots += root;
                }
                if (nTips && nRoots)
                {
                    for (int k = 0; k < 4; k++) { bladeTip[k] = tips[k] / nTips; bladeRoot[k] = roots[k] / nRoots; }
                    haveBlades = true;
                }
            }
            if ((t != "chip_kusa_a" && t != "chip_kusa_b") || m.Vertices.empty()) continue;
            // the main grass is painted dark under trees and cliffs: its open ground is the brighter half of its vertices
            std::vector<float> light;
            for (const BchVertex& v : m.Vertices) light.push_back(v.Colour[0] + v.Colour[1] + v.Colour[2]);
            std::sort(light.begin(), light.end());
            const float median = light[light.size() / 2];
            const bool brighterHalf = t == "chip_kusa_a";
            float mean[4] = {0, 0, 0, 0};
            size_t used = 0;
            for (const BchVertex& v : m.Vertices)
            {
                if (brighterHalf && v.Colour[0] + v.Colour[1] + v.Colour[2] < median) continue;
                for (int k = 0; k < 4; k++) mean[k] += v.Colour[k];
                used++;
            }
            for (float& x : mean) x /= used;
            std::copy(mean, mean + 4, t == "chip_kusa_a" ? colourA : colourB);
            (t == "chip_kusa_a" ? haveA : haveB) = true;
        }
        ownGrass = haveA && haveB;
        if (ownGrass) { std::copy(colourA, colourA + 4, grass); std::copy(colourB, colourB + 4, white); }
        note("target's own grass: %s (grass %.2f %.2f %.2f, light %.2f %.2f %.2f)\n", ownGrass ? "found" : "NOT found, the donor's grass kept", grass[0], grass[1], grass[2], white[0], white[1], white[2]);
    }
    float forest[4]; for (int k = 0; k < 3; k++) forest[k] = grass[k] * 0.7f; forest[3] = grass[3];
    const std::vector<std::string>& path2 = layout.Path2;
    const std::vector<std::string>& water2 = layout.Water2;
    auto fineVis = [&](int c, int r) { return vis[r / 2][c / 2]; };
    // the town's ground (ORAS's light grass, nearest Platinum's) and its paths at half-tile precision
    // the pond's frame (f) is grass: Platinum has no sand around its pond
    if (ownGrass)
    {
        // The lighter grass patches and the paths are cut as ORAS cuts them (TownShapes) and laid just over a main grass that covers
        // the whole open ground, so a corner cut off a zone shows grass, not a hole
        auto open = [&](int c, int r, const char* classes) { return path2[r][c] != ':' && water2[r][c] != '~' && std::string(classes).find(fineVis(c, r)) != std::string::npos; };
        auto lightGrass = [&](int c, int r) { return open(c, r, "s"); };
        auto path = [&](int c, int r) { return path2[r][c] == ':' && water2[r][c] != '~'; };
        FlatFine(geo[Ground], 2, [&](int c, int r) { return water2[r][c] != '~' && (path(c, r) || open(c, r, ".*HtF:f~s")); }, 0, grass);
        // ORAS's zones follow the tile lattice (ORAS_LITTLEROOT.md 9d, 9e): the lighter grass is made of whole tiles (a tile is in it
        // when at least two of its four halves are, a tie going to the path); a path may step by half tiles, as 94 of the game's 286
        // path meshes do, which is Platinum's own precision
        const int M = 2 * N;
        auto tileMask = [&](auto zone, auto other) {
            std::vector<std::vector<bool>> mask(N, std::vector<bool>(N, false));
            for (int r = 0; r < N; r++)
                for (int c = 0; c < N; c++)
                {
                    int mine = 0, theirs = 0;
                    for (int k = 0; k < 4; k++) { mine += zone(2 * c + (k & 1), 2 * r + (k >> 1)); theirs += other(2 * c + (k & 1), 2 * r + (k >> 1)); }
                    mask[r][c] = mine >= 2 && theirs < 2;
                }
            return mask;
        };
        std::vector<std::vector<bool>> pathMask(M, std::vector<bool>(M, false));
        for (int r = 0; r < M; r++) for (int c = 0; c < M; c++) pathMask[r][c] = path(c, r);
        const float corner = -20 * T; // the window's corner (X(0), Z(0))
        // the lighter grass as Littleroot's is cut, measured: no jitter, its own corners 0.12 tile in (TownShapes.h)
        const bool snowy = !src.SnowTexture.empty();
        const ZoneShape lightShape = StairZone(tileMask(lightGrass, path), T, corner, corner, false, 0, 0.12f);
        // snow: the half tiles Platinum shows white (TownLayout's Snow2), so its round blobs keep their shape; half-tile steps as
        // ORAS's paths may take (94 of the game's 286 path meshes), on the lattice exactly as the ice cave's snow (piece 386,
        // mesh 7: 217 of its 270 vertices, median 0.001 tile off), no pull
        std::vector<std::vector<bool>> snowMask(M, std::vector<bool>(M, false));
        for (int r = 0; r < M; r++) for (int c = 0; c < M; c++) snowMask[r][c] = layout.Snow2[r][c] == '#';
        const ZoneShape snowShape = StairZone(snowMask, T / 2, corner, corner, false, 0, 0);
        const ZoneShape pathShape = StairZone(pathMask, T / 2, corner, corner, false);
        // Platinum's snow patches (role s) are these zones. With a snow texture they are laid as ORAS lays the snow of its ice cave
        // (piece 386, mesh 7, chip_icedoukutsu02): its texture, planar mapping and vertex colour 0.95 1 1, in a blended layer (the
        // donor's field slot, mesh 22, layer 1), 0.45 up, over the outline strips (0.3), with no outline of its own. The cave's snow
        // is translucent (alpha 0.57 inside and on its open border, 1.0 against the walls, measured) because it lies on white
        // ice; on green grass that would show pale green, so it is opaque here (the owner's choice): white, as Platinum's
        const float snow[4] = {0.95f, 1, 1, 1};
        // (in the lighter grass's opaque slot: the snow is opaque; the blended slot 22 carries its edge clumps, SnowClumps)
        if (snowy) AddFill(geo[Pale], snowShape, 0.45f, snow);
        if (snowy && !src.SnowClumpTexture.empty())
        {
            // open ground: not a tree, the forest or a house (tile roles), off the window counting as forest
            auto open = [&](float x, float z) {
                const int c = (int)std::floor(x / T + N / 2), r = (int)std::floor(z / T + N / 2);
                return c >= 0 && r >= 0 && c < N && r < N && std::string("tTH").find(vis[r][c]) == std::string::npos;
            };
            for (const ShapeChain& chain : snowShape.Chains) SnowClumps(geo[SnowBand], chain, open);
        }
        else AddFill(geo[Pale], lightShape, 0.15f, white);
        AddFill(geo[Soil], pathShape, 0.15f, soil);
        // the outline of each zone, on its border, coloured as the target's own outline (one colour for tips and roots, the
        // grass's less 15%, put a dark band over the lighter grass and the paths); the grass's less 15% when it has none
        const float blade[4] = {grass[0] * 0.85f, grass[1] * 0.85f, grass[2] * 0.85f, grass[3]};
        // the strip on each zone's border, its tips and roots placed as ORAS places them (TownShapes.h, OutlineStrip); snow has none
        for (const ZoneShape* shape : {&lightShape, &pathShape})
        {
            if (shape == &lightShape && snowy) continue;
            for (const ShapeChain& chain : shape->Chains)
                AddOutline(geo[Outline], chain, haveBlades ? bladeTip : blade, haveBlades ? bladeRoot : blade);
        }
        note("outline blades: %s (tips %.2f %.2f %.2f, roots %.2f %.2f %.2f)\n", haveBlades ? "the target's" : "NOT found, the grass's less 15%",
             (haveBlades ? bladeTip : blade)[0], (haveBlades ? bladeTip : blade)[1], (haveBlades ? bladeTip : blade)[2],
             (haveBlades ? bladeRoot : blade)[0], (haveBlades ? bladeRoot : blade)[1], (haveBlades ? bladeRoot : blade)[2]);
        // the rim where walkable ground meets solid trees and forest, from a mask of everything else
        std::vector<std::vector<bool>> notWall(N, std::vector<bool>(N, true));
        for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) notWall[r][c] = !(coll[r][c] == '#' && (vis[r][c] == 't' || vis[r][c] == 'T'));
        auto snowAt = [&](float x, float z) {
            const int c = (int)std::floor(x / (T / 2) + N), r = (int)std::floor(z / (T / 2) + N);
            return snowy && c >= 0 && r >= 0 && c < M && r < M && layout.Snow2[r][c] == '#';
        };
        for (const ShapeChain& chain : StairZone(notWall, T, corner, corner, true).Chains) AddRim(geo[Edge], chain, snowAt);
        // decals on plain open grass: grass-role tiles away from paths, water, houses and fences
        // (not on snow: grass decals do not grow there)
        auto plain = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && (vis[r][c] == '.' || (vis[r][c] == 's' && src.SnowTexture.empty())) && coll[r][c] == '.' && path2[2 * r][2 * c] != ':' && path2[2 * r + 1][2 * c + 1] != ':'; };
        const int decals = GrassDecals(geo[Outline], plain);
        note("grass edge: %zu outlines, %d decals\n", (snowy ? 0 : lightShape.Chains.size()) + pathShape.Chains.size(), decals);
    }
    else
        FlatFine(geo[Pale], 2, [&](int c, int r) { return path2[r][c] != ':' && water2[r][c] != '~' && std::string(".*HstF:f~").find(fineVis(c, r)) != std::string::npos; }, 0, white);
    if (!ownGrass) FlatFine(geo[Soil], 2, [&](int c, int r) { return path2[r][c] == ':' && water2[r][c] != '~'; }, 0, soil);
    Flat(geo[Ground], vis, "T", 0, forest, 12);
    // the pond from Platinum's colours (its blue edge, lakep, is water too)
    FlatFine(geo[Water], 2, [&](int c, int r) { return water2[r][c] == '~'; }, -4.2f, waterColour);
    if (src.BankTexture.empty()) BanksFine(geo[Bank], water2);
    else BanksFine(geo[Bank], water2, src.BankV[0], src.BankV[1]);
    // no outline on the paths (the owner's choice): the outline mesh is left empty
    int nFlowers = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
        if (vis[r][c] == '*') { for (auto& [m, p] : flowers) Place(geo[m], p, c + 0.5f, r + 0.5f); nFlowers++; }

    // fences: a low Petalburg hedge (one tile of its mesh 7 run at column 37, rows 1-6) on each fence tile,
    // turned along the fence where it runs east-west
    // (with a fence texture, ORAS's white picket fence instead, AddFence, in the same mesh slot)
    const Part hedge = Cut(pm.Meshes[Hedge], 36.9f, 38.1f, 3.0f, 4.0f, 37.5f, 3.5f);
    int nHedges = 0;
    auto fence = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && vis[r][c] == 'F'; };
    if (!src.FenceTexture.empty())
    {
        AddFence(geo[Hedge], vis, N, -20 * T);
        note("fence: %zu panel triangles\n", geo[Hedge].Triangles.size() / 3);
    }
    else for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
        if (fence(c, r))
        {
            const bool eastWest = (fence(c - 1, r) || fence(c + 1, r)) && !(fence(c, r - 1) || fence(c, r + 1));
            Place(geo[Hedge], hedge, c + 0.5f, r + 0.5f, 1, eastWest, 0.55f);
            nHedges++;
        }
    if (src.FenceTexture.empty()) note("%d hedge tiles (%zu triangles each)\n", nHedges, hedge.I.size() / 3);

    // houses: each DS house's solid tiles (joined to its door) give its width; Petalburg's house is 5 wide
    std::vector<std::pair<int, int>> houseTiles;
    for (const auto& d : doors)
    {
        int c0 = d[0], c1 = d[0];
        while (c0 > 0 && coll[d[1]][c0 - 1] == '#') c0--;
        while (c1 < N - 1 && coll[d[1]][c1 + 1] == '#') c1++;
        const float scale = (c1 - c0 + 1) / 5.0f;
        // Petalburg's front stands 0.15 tile further south of its door than Platinum's (its door tile's south edge)
        for (auto& [m, p] : house) Place(geo[m], p, d[0] + 0.5f, d[1] + 0.5f - 0.15f * scale, scale);
        note("house: door (%d, %d), %d tiles wide, scale %.2f\n", d[0], d[1], c1 - c0 + 1, scale);
    }

    // trees: one Route 101 tree on each corner whose four tiles Platinum shows as trees, 2 tiles apart,
    // within 6 tiles of the town, none over a house
    auto isTree = [&](int c, int r) { if (c < 0 || r < 0 || c >= N || r >= N) return true; const char ch = vis[r][c]; return ch == 'T' || ch == 't'; };
    struct Spot { int c, r, d; };
    std::vector<Spot> spots;
    for (int r = -2; r <= N + 2; r++)
        for (int c = -2; c <= N + 2; c++)
        {
            if (!isTree(c - 1, r - 1) || !isTree(c, r - 1) || !isTree(c - 1, r) || !isTree(c, r)) continue;
            int d = 99;
            for (int dr = -7; dr <= 6; dr++) for (int dc = -7; dc <= 6; dc++)
                if (!isTree(c + dc, r + dr)) d = std::min(d, std::max(std::abs(dc + (dc < 0)), std::abs(dr + (dr < 0))));
            bool nearWater = false; // no tree overhanging the pond
            for (int dr = -2; dr <= 2; dr++) for (int dc = -2; dc <= 2; dc++)
            {
                const int fc = 2 * c + dc, fr = 2 * r + dr;
                if (fc >= 0 && fr >= 0 && fc < 2 * N && fr < 2 * N && water2[fr][fc] == '~') nearWater = true;
            }
            // within 3 tiles of open ground: with 4 the piece passes the game's largest (35,691 vertices, 'oras-town' warns)
            if (d <= src.TreeReach && !nearWater) spots.push_back({c, r, d});
        }
    std::stable_sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) { return a.d < b.d; });
    // the tree's two upper leaf layers (Route 101's layers span y 27-52, 43-68, 54-79, 71-96)
    Part treeTop;
    {
        const Part& all = tree.at(Canopy);
        std::map<uint32_t, uint32_t> remap;
        for (size_t t = 0; t + 2 < all.I.size(); t += 3)
        {
            float y = 0; for (int k = 0; k < 3; k++) y = std::max(y, all.V[all.I[t + k]].Position[1]);
            if (y < 75) continue;
            for (int k = 0; k < 3; k++)
            {
                auto it = remap.find(all.I[t + k]);
                if (it == remap.end()) { it = remap.emplace(all.I[t + k], (uint32_t)treeTop.V.size()).first; treeTop.V.push_back(all.V[all.I[t + k]]); }
                treeTop.I.push_back(it->second);
            }
        }
        size_t full = 0; for (auto& [m, p] : tree) full += p.I.size() / 3;
        note("tree: %zu triangles whole, %zu its upper leaves\n", full, treeTop.I.size() / 3);
    }
    std::vector<Spot> trees;
    for (const Spot& s : spots)
    {
        bool clear = true;
        for (const Spot& t : trees) if ((t.c - s.c) * (t.c - s.c) + (t.r - s.r) * (t.r - s.r) < 2) { clear = false; break; }
        if (!clear) continue;
        trees.push_back(s);
        // the forest's edge (within 1 tile of open ground) gets whole trees; further in, where trunks and lower
        // leaves are hidden by the trees in front, only the two upper layers of leaves
        if (s.d <= 1) for (auto& [m, p] : tree) Place(geo[m], p, (float)s.c, (float)s.r, 0.65f);
        else Place(geo[Canopy], treeTop, (float)s.c, (float)s.r, 0.65f);
    }

    // collision: Platinum's permissions; its pond as Petalburg's water
    BinLinker gr = BinLinker::Read(lr, "GR");
    Bytes& tiles = gr.Files[0];
    // a lone town's piece closes its bottom row and side columns; a region's pieces do not (they joined their neighbours with walls
    // across Twinleaf's crossroads on the phone, r4: the seam between two pieces)
    if (src.CloseEdges)
        for (int i = 0; i < N; i++) { coll[N - 1][i] = coll[N - 1][i] == '~' ? '~' : '#'; coll[i][0] = coll[i][N - 1] = '#'; }
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
    {
        const char ch = coll[r][c];
        // the value ORAS gives the surface under a tile (tile_surfaces.tsv, ORAS_LITTLEROOT.md 9e): the most used of the game's for solid (842 pieces),
        // water (173), a path (81, 95% of its tiles lie under a path mesh) and tall grass (59), plain ground otherwise
        int pathHalves = 0;
        for (int k = 0; k < 4; k++) pathHalves += layout.Path2[2 * r + (k >> 1)][2 * c + (k & 1)] == ':';
        const uint32_t v = ch == 'v' ? 0x75000021 : ch == '#' ? 0x01000021 : ch == '~' ? 0x3d1a0006 : ch == 'g' ? 0x20004004 : pathHalves >= 2 ? 0x020a8020 : 0x00000020;
        for (int k = 0; k < 4; k++) tiles[4 + (r * N + c) * 4 + k] = (uint8_t)(v >> (8 * k));
    }
    // door models: the target's own entries (its door types, scale, height, rotation and unknown words), each moved onto a
    // door tile's centre, as all 368 door models of the game sit (ORAS_LITTLEROOT.md 9b). Writing Petalburg's door type (4)
    // with every other word zeroed hid the whole map on the phone (t17: the built model with these doors shows nothing, with
    // Littleroot's own doors it shows); which of the type or the zeroed words the game refused is not known
    Bytes& dm = gr.Files[3];
    const Bytes own = dm;
    auto get = [&](size_t at) { uint32_t v = 0; for (int k = 0; k < 4; k++) v |= (uint32_t)own.at(at + k) << (8 * k); return v; };
    auto put = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) dm.at(at + k) = (uint8_t)(v >> (8 * k)); };
    auto putf = [&](size_t at, float f) { uint32_t v; memcpy(&v, &f, 4); put(at, v); };
    const size_t ownCount = get(0);
    if (ownCount == 0) throw FormatError("the target piece has no door model to place");
    const size_t placed = std::min(doors.size(), (dm.size() - 4) / 44);
    put(0, (uint32_t)placed);
    for (size_t k = 0; k < placed; k++)
    {
        const size_t e = 4 + k * 44, from = 4 + std::min(k, ownCount - 1) * 44;
        std::copy(own.begin() + from, own.begin() + from + 44, dm.begin() + e);
        if (src.DoorType) put(e, src.DoorType);
        putf(e + 28, (float)((src.CellX * N + doors[k][0]) * 18 + 9)); putf(e + 36, (float)((src.CellY * N + doors[k][1]) * 18 + 9));
    }

    std::vector<BchGeometry> list;
    size_t verts = 0;
    for (auto& [m, g] : geo) { if (g.Vertices.empty()) g.Vertices.resize(1); verts += g.Vertices.size(); list.push_back(std::move(g)); }
    // the outline's material (3, chip_grass_decolate) shows r120_alpha, in area pack 9, in place of touka_alpha
    // the terrain model's name tells its place (world<matrix>_<x>_<y>): Petalburg's world02_02_03 becomes
    // Littleroot's world01_02_04, every copy (model and nodes)
    Bytes terrain = BchReplaceString(BchReplaceGeometry(petalTerrain, 0, list), pm.Name,
                                     src.ModelName.empty() ? Bch::Read(BinLinker::Read(lr, "GR").Files[1]).Models[0].Name : src.ModelName);
    auto show = [&](size_t mesh, int slot, const std::string& texture) {
        const BchMaterial& m = pm.Materials[pm.Meshes[mesh].Material];
        if (!texture.empty() && m.Texture[slot] != texture) terrain = BchSetTextureName(terrain, 0, pm.Meshes[mesh].Material, slot, texture);
    };
    // the outline mesh's material shows chip_alpha (grass blades), as Littleroot's does, in place of Petalburg's touka_alpha
    if (ownGrass)
    {
        terrain = BchSetTextureName(terrain, 0, pm.Meshes[Outline].Material, 0, "chip_alpha");
        show(Ground, 0, src.GroundTexture);
        show(Pale, 0, src.SnowTexture.empty() ? src.LightTexture : src.SnowTexture);
        show(SnowBand, 0, src.SnowClumpTexture);
        show(Edge, 1, src.EdgeTexture);
    }
    // the pond's walls and the fence are built whatever the grass (BanksFine with the texture's rows, AddFence), so their
    // textures are shown whatever the grass too (with --grass -1 they kept the donor's rock and hedge, mapped for others)
    show(Bank, 0, src.BankTexture);
    show(Hedge, 0, src.FenceTexture);
    if (nLedges) show(SnowBand, 0, "chip_jump_gake");
    gr.Files[1] = terrain;
    // file 4 opens with the piece's own matrix cell, row then column (Littleroot's 04 02 at world01_02_04; checked on Hoenn's
    // pieces). The game files its piece-slot table (4 slots, code 0x3C8A24) under that word and frees a slot only for the
    // cell leaving the player's window (0x3C8C84): a piece carrying Littleroot's cell was never freed, so the fifth piece
    // loaded hit "no free slot", the fatal-error loop of every Route 201 freeze (ORAS_LITTLEROOT.md 0)
    // a model's distinct textures: the engine stops past 51 (FUN_0048883c, `0x33 <` its texture-slot entries, 3 a material;
    // read, ORAS_ENGINE.md 4.3; that it governs map pieces is inferred). Hoenn's pieces use at most 45, a built one 29
    {
        std::set<std::string> textures;
        for (const BchModel& m : Bch::Read(gr.Files.at(1)).Models)
            for (const BchMaterial& mat : m.Materials)
                for (const std::string& t : mat.Texture) if (!t.empty()) textures.insert(t);
        if (textures.size() > 51) throw FormatError("the piece's model names " + std::to_string(textures.size()) + " textures, the game stops past 51");
    }
    Bytes& cell = gr.Files.at(4);
    if (cell.size() < 2) throw FormatError("piece file 4 holds no cell");
    cell[0] = (uint8_t)src.CellY;
    cell[1] = (uint8_t)src.CellX;
    note("%zu trees, %d flower patches, %zu vertices; terrain model %zu bytes (Petalburg's %zu, Littleroot's %zu)\n", trees.size(), nFlowers, verts,
         gr.Files[1].size(), petalTerrain.size(), BinLinker::Read(lr, "GR").Files[1].size());
    const Bytes out = gr.Write();
    note("piece %zu bytes\n", out.size());
    return out;
}

}
