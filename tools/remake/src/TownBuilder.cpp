#include "TownBuilder.h"
#include "BchWriter.h"
#include "BinLinker.h"
#include "TownShapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

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
static void BanksFine(BchGeometry& g, const std::vector<std::string>& water2)
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
            v.TexCoord[0] = (e ? len : 0) / 36.0f; v.TexCoord[1] = 0.843f - y / 100.0f; g.Vertices.push_back(v);
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

// The grass's soft edge around a zone (its outline), laid on the zone's rounded border, centred on it: 9 units wide, 4.5 into
// the zone and 4.5 out, 0.3 above the ground, with Littleroot's chip_grass_decolate mesh's width, height and texture band
// (measured). The blades are chip_alpha's grass band (rows 38-60 of 128): the tips (v 0.302, white) into the zone, the roots
// (v 0.496, the grass's colour) out on the grass, so the grass's edge is jagged and runs on beyond it. u advances 0.0245 a
// unit along the border, wrapping inside the band (u 0.046-0.453), the strip cut where it wraps.
static void AddOutline(BchGeometry& g, const ShapeChain& chain, const float tip[4], const float root[4])
{
    const float half = 4.5f, perUnit = 0.0245f, lo = 0.046f, span = 0.407f;
    float s = 0;
    const size_t n = chain.Points.size();
    const size_t last = chain.Closed ? n : n - 1;
    auto vertex = [&](const ShapePoint& p, const ShapePoint& nrm, bool tipSide, float u) {
        BchVertex v;
        const float off = tipSide ? -half : half;
        v.Position[0] = p.X + nrm.X * off; v.Position[1] = 0.3f; v.Position[2] = p.Z + nrm.Z * off;
        v.TexCoord[0] = u; v.TexCoord[1] = tipSide ? 0.302f : 0.496f;
        std::copy(tipSide ? tip : root, (tipSide ? tip : root) + 4, v.Colour);
        g.Vertices.push_back(v);
    };
    for (size_t i = 0; i < last; i++)
    {
        const ShapePoint &a = chain.Points[i], &b = chain.Points[(i + 1) % n], &na = chain.Normals[i], &nb = chain.Normals[(i + 1) % n];
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
            auto along = [&](float d, ShapePoint& p, ShapePoint& nrm) {
                const float t = d / length;
                p = {lerp(a.X, b.X, t), lerp(a.Z, b.Z, t)};
                nrm = {lerp(na.X, nb.X, t), lerp(na.Z, nb.Z, t)};
                const float m = std::hypot(nrm.X, nrm.Z);
                if (m > 1e-6f) { nrm.X /= m; nrm.Z /= m; }
            };
            ShapePoint p0, n0, p1, n1;
            along(at, p0, n0); along(to, p1, n1);
            const uint32_t base = (uint32_t)g.Vertices.size();
            const float u1 = u0 + (to - at) * perUnit;
            vertex(p0, n0, true, u0); vertex(p0, n0, false, u0); vertex(p1, n1, true, u1); vertex(p1, n1, false, u1);
            // facing up, as the ground (see the quads': a, e, d / a, d, b); the zone is on the tips' side
            const float cross = (p1.X - p0.X) * n0.Z - (p1.Z - p0.Z) * n0.X;
            if (cross > 0) for (uint32_t k : {0u, 2u, 3u, 0u, 3u, 1u}) g.Triangles.push_back(base + k);
            else for (uint32_t k : {0u, 3u, 2u, 0u, 1u, 3u}) g.Triangles.push_back(base + k);
            at = to;
        }
        s += length;
    }
}

// The rim of the playable ground, as Littleroot lays it (its chip_edge_tex mesh, measured): a strip along the border where
// walkable ground meets the solid trees and forest around it (the border of the rounded walkable zone), 9 units wide on
// the solid side, rising from 1 at the border to 3.5 outside, in the rim's blue-green. The texture is projected by the
// material (chip_grass_edge, from the stored positions), so the coordinates are Littleroot's own.
static void AddRim(BchGeometry& g, const ShapeChain& chain)
{
    const float colour[4] = {0.26f, 0.75f, 0.87f, 1.0f};
    const size_t n = chain.Points.size();
    const size_t last = chain.Closed ? n : n - 1;
    for (size_t i = 0; i < last; i++)
    {
        const ShapePoint &a = chain.Points[i], &b = chain.Points[(i + 1) % n], &na = chain.Normals[i], &nb = chain.Normals[(i + 1) % n];
        if (std::hypot(b.X - a.X, b.Z - a.Z) < 1e-4f) continue;
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
           Hedge = 7, Trunk = 19, Outline = 20, Edge = 21, Wall = 24, Water = 25, Shadow = 26 };

    // kits: Petalburg's house at (24-28, 21-25), its door (25, 25) the anchor; a flower patch; Route 101's tree
    std::map<int, Part> house;
    for (int m : {House, Frame, Window, Wall}) house[m] = Cut(pm.Meshes[m], 23.5f, 29.5f, 21.4f, 26.7f, 25.5f, 25.5f);
    std::map<int, Part> flowers = {{FlowerA, Cut(pm.Meshes[FlowerA], 21.0f, 22.2f, 24.8f, 25.9f, 21.5f, 25.4f)},
                                   {FlowerB, Cut(pm.Meshes[FlowerB], 21.0f, 22.2f, 24.8f, 25.9f, 21.5f, 25.4f)}};
    std::map<int, Part> tree = {{Canopy, Cut(rm.Meshes[0], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f)},
                                {Trunk, Cut(rm.Meshes[5], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f)},
                                {Shadow, Cut(rm.Meshes[9], 6.3f, 11.0f, 23.0f, 27.0f, 8.6f, 25.1f)}};
    for (auto& [m, p] : house) note("house kit mesh %d: %zu triangles\n", m, p.I.size() / 3);

    std::map<size_t, BchGeometry> geo;
    for (size_t m = 0; m < pm.Meshes.size(); m++) geo[m].Mesh = m;
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
    bool ownGrass = false;
    if (src.TargetGrass)
    {
        const BchModel tm = Bch::Read(BinLinker::Read(lr, "GR").Files[1]).Models[0];
        bool haveA = false, haveB = false;
        float colourA[4] = {}, colourB[4] = {};
        for (const BchMesh& m : tm.Meshes)
        {
            const std::string& t = tm.Materials[m.Material].Texture[0];
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
        // Zones with rounded corners: the lighter grass patches and the paths are cut from a blurred mask (TownShapes) and
        // laid just over a main grass that covers the whole open ground, so a rounded corner shows grass, not a hole
        const int M = 2 * N;
        auto open = [&](int c, int r, const char* classes) { return path2[r][c] != ':' && water2[r][c] != '~' && std::string(classes).find(fineVis(c, r)) != std::string::npos; };
        auto lightGrass = [&](int c, int r) { return open(c, r, "s"); };
        auto path = [&](int c, int r) { return path2[r][c] == ':' && water2[r][c] != '~'; };
        FlatFine(geo[Ground], 2, [&](int c, int r) { return water2[r][c] != '~' && (path(c, r) || open(c, r, ".*HtF:f~s")); }, 0, grass);
        auto maskOf = [&](auto zone) {
            std::vector<std::vector<bool>> mask(M, std::vector<bool>(M, false));
            for (int r = 0; r < M; r++) for (int c = 0; c < M; c++) mask[r][c] = zone(c, r);
            return mask;
        };
        const float cell = T / 2, corner = -20 * T; // a half tile; the window's corner (X(0), Z(0))
        const ZoneShape lightShape = SmoothZone(maskOf(lightGrass), cell, corner, corner, 1);
        const ZoneShape pathShape = SmoothZone(maskOf(path), cell, corner, corner, 1);
        AddFill(geo[Pale], lightShape, 0.15f, white);
        AddFill(geo[Soil], pathShape, 0.15f, soil);
        // the outline of each zone, on its border
        // the blades' texel colour is brighter than the ground texture's mean, so their vertex colour is the grass's less 15%:
        // the base fades into the grass without a lighter line
        const float blade[4] = {grass[0] * 0.85f, grass[1] * 0.85f, grass[2] * 0.85f, grass[3]};
        for (const ZoneShape* shape : {&lightShape, &pathShape})
            for (const ShapeChain& chain : shape->Chains) AddOutline(geo[Outline], chain, blade, blade);
        // the rim where walkable ground meets solid trees and forest, from a mask of everything else
        std::vector<std::vector<bool>> notWall(N, std::vector<bool>(N, true));
        for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) notWall[r][c] = !(coll[r][c] == '#' && (vis[r][c] == 't' || vis[r][c] == 'T'));
        for (const ShapeChain& chain : SmoothZone(notWall, T, corner, corner, 1).Chains) AddRim(geo[Edge], chain);
        // decals on plain open grass: grass-role tiles away from paths, water, houses and fences
        auto plain = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && (vis[r][c] == '.' || vis[r][c] == 's') && coll[r][c] == '.' && path2[2 * r][2 * c] != ':' && path2[2 * r + 1][2 * c + 1] != ':'; };
        const int decals = GrassDecals(geo[Outline], plain);
        note("grass edge: %zu outlines, %d decals\n", lightShape.Chains.size() + pathShape.Chains.size(), decals);
    }
    else
        FlatFine(geo[Pale], 2, [&](int c, int r) { return path2[r][c] != ':' && water2[r][c] != '~' && std::string(".*HstF:f~").find(fineVis(c, r)) != std::string::npos; }, 0, white);
    if (!ownGrass) FlatFine(geo[Soil], 2, [&](int c, int r) { return path2[r][c] == ':' && water2[r][c] != '~'; }, 0, soil);
    Flat(geo[Ground], vis, "T", 0, forest, 12);
    // the pond from Platinum's colours (its blue edge, lakep, is water too)
    FlatFine(geo[Water], 2, [&](int c, int r) { return water2[r][c] == '~'; }, -4.2f, waterColour);
    BanksFine(geo[Bank], water2);
    // no outline on the paths (the owner's choice): the outline mesh is left empty
    int nFlowers = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
        if (vis[r][c] == '*') { for (auto& [m, p] : flowers) Place(geo[m], p, c + 0.5f, r + 0.5f); nFlowers++; }

    // fences: a low Petalburg hedge (one tile of its mesh 7 run at column 37, rows 1-6) on each fence tile,
    // turned along the fence where it runs east-west
    const Part hedge = Cut(pm.Meshes[Hedge], 36.9f, 38.1f, 3.0f, 4.0f, 37.5f, 3.5f);
    int nHedges = 0;
    auto fence = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && vis[r][c] == 'F'; };
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
        if (fence(c, r))
        {
            const bool eastWest = (fence(c - 1, r) || fence(c + 1, r)) && !(fence(c, r - 1) || fence(c, r + 1));
            Place(geo[Hedge], hedge, c + 0.5f, r + 0.5f, 1, eastWest, 0.55f);
            nHedges++;
        }
    note("%d hedge tiles (%zu triangles each)\n", nHedges, hedge.I.size() / 3);

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
            if (d <= 4 && !nearWater) spots.push_back({c, r, d});
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
    for (int i = 0; i < N; i++) { coll[N - 1][i] = coll[N - 1][i] == '~' ? '~' : '#'; coll[i][0] = coll[i][N - 1] = '#'; }
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++)
    {
        const char ch = coll[r][c];
        const uint32_t v = ch == '#' ? 0x01000021 : ch == '~' ? 0x3d180006 : ch == 'g' ? 0x20004004 : 0x00000020;
        for (int k = 0; k < 4; k++) tiles[4 + (r * N + c) * 4 + k] = (uint8_t)(v >> (8 * k));
    }
    // door models: Petalburg's house door (type 4) on each door, scaled with its house
    Bytes& dm = gr.Files[3];
    std::fill(dm.begin(), dm.end(), 0);
    auto put = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) dm.at(at + k) = (uint8_t)(v >> (8 * k)); };
    auto putf = [&](size_t at, float f) { uint32_t v; memcpy(&v, &f, 4); put(at, v); };
    put(0, (uint32_t)doors.size());
    for (size_t k = 0; k < doors.size(); k++)
    {
        int c0 = doors[k][0], c1 = doors[k][0];
        while (c0 > 0 && coll[doors[k][1]][c0 - 1] == '#') c0--;
        while (c1 < N - 1 && coll[doors[k][1]][c1 + 1] == '#') c1++;
        const float scale = (c1 - c0 + 1) / 5.0f;
        const size_t e = 4 + k * 44;
        put(e, 4); putf(e + 4, scale); putf(e + 8, scale); putf(e + 12, scale);
        putf(e + 28, (float)((src.CellX * N + doors[k][0]) * 18 + 9)); putf(e + 36, (float)((src.CellY * N + doors[k][1]) * 18 + 9));
    }

    std::vector<BchGeometry> list;
    size_t verts = 0;
    for (auto& [m, g] : geo) { if (g.Vertices.empty()) g.Vertices.resize(1); verts += g.Vertices.size(); list.push_back(std::move(g)); }
    // the outline's material (3, chip_grass_decolate) shows r120_alpha, in area pack 9, in place of touka_alpha
    // the terrain model's name tells its place (world<matrix>_<x>_<y>): Petalburg's world02_02_03 becomes
    // Littleroot's world01_02_04, every copy (model and nodes)
    Bytes terrain = BchReplaceString(BchReplaceGeometry(petalTerrain, 0, list), pm.Name, Bch::Read(BinLinker::Read(lr, "GR").Files[1]).Models[0].Name);
    // the outline mesh's material shows chip_alpha (grass blades), as Littleroot's does, in place of Petalburg's touka_alpha
    if (ownGrass) terrain = BchSetTextureName(terrain, 0, pm.Meshes[Outline].Material, 0, "chip_alpha");
    gr.Files[1] = terrain;
    note("%zu trees, %d flower patches, %zu vertices; terrain model %zu bytes (Petalburg's %zu, Littleroot's %zu)\n", trees.size(), nFlowers, verts,
         gr.Files[1].size(), petalTerrain.size(), BinLinker::Read(lr, "GR").Files[1].size());
    const Bytes out = gr.Write();
    note("piece %zu bytes\n", out.size());
    return out;
}

}
