#include "RoomBuilder.h"

#include "Bch.h"
#include "BchWriter.h"
#include "BinLinker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <tuple>

namespace remake
{

// measured on the pieces (scratch scan of their meshes' connected parts, 9 October): t101r0101 (506) has its plain floor at
// tile (15, 12), its back wall on row 6 (plain at column 11), its west wall on column 6 (plain at row 9), its east wall on
// column 19 (plain at row 16), and its stairs up at columns 15.66-18.34, rows 7-10 (side walls included), rising north from
// row 10, their warp on tile (16, 9) (zone 223's warp to 224). t101r0102 (507): floor at (10, 10), back wall on row 6.5 (plain
// at column 9), west wall on column 6 (plain at row 9), east wall on column 18 (plain at row 11), the stairs down an opening
// at columns 14-16.5, rows 6.5-9 with its railing, its warp on tile (14, 7) (zone 224's warp to 223)
const RoomDonor GroundFloorRoom = {506, 112, 15, 12, 6.0f, 11, 6.0f, 9, 19.0f, 16, {15.6f, 18.4f, 6.9f, 10.05f}, 16, 9};
const RoomDonor UpstairsRoom = {507, 112, 10, 10, 6.5f, 9, 6.0f, 9, 18.0f, 11, {13.6f, 18.1f, 6.4f, 9.98f}, 14, 7};

namespace
{

constexpr int N = TownTiles;
constexpr float T = 18; // units a tile
float X(float column) { return (column - 20) * T; }
float Z(float row) { return (row - 20) * T; }
float Column(float x) { return x / T + 20; }
float Row(float z) { return z / T + 20; }

using Polygon = std::vector<BchVertex>;

BchVertex Lerp(const BchVertex& a, const BchVertex& b, float t)
{
    BchVertex v;
    for (int k = 0; k < 3; k++) { v.Position[k] = a.Position[k] + (b.Position[k] - a.Position[k]) * t; v.Normal[k] = a.Normal[k] + (b.Normal[k] - a.Normal[k]) * t; }
    for (int k = 0; k < 2; k++) v.TexCoord[k] = a.TexCoord[k] + (b.TexCoord[k] - a.TexCoord[k]) * t;
    for (int k = 0; k < 4; k++) v.Colour[k] = a.Colour[k] + (b.Colour[k] - a.Colour[k]) * t;
    return v;
}

// the polygon's part with position[axis] on the kept side of the plane at `at` (keepAbove: >= at)
Polygon ClipPlane(const Polygon& p, int axis, float at, bool keepAbove)
{
    Polygon out;
    for (size_t i = 0; i < p.size(); i++)
    {
        const BchVertex &a = p[i], &b = p[(i + 1) % p.size()];
        const float da = keepAbove ? a.Position[axis] - at : at - a.Position[axis], db = keepAbove ? b.Position[axis] - at : at - b.Position[axis];
        if (da >= 0) out.push_back(a);
        if ((da >= 0) != (db >= 0)) out.push_back(Lerp(a, b, da / (da - db)));
    }
    return out;
}

// the mesh's triangles clipped to a box (in units; axis 0 x, 2 z), as a fan per clipped triangle; keep: decides on the triangle's
// centre (in tiles) whether it is taken at all
template <typename Keep>
void ClipMesh(const BchMesh& m, float x0, float x1, float z0, float z1, Keep keep, Polygon& outVertices, std::vector<uint32_t>& outTriangles)
{
    for (size_t t = 0; t + 2 < m.Triangles.size(); t += 3)
    {
        Polygon p = {m.Vertices[m.Triangles[t]], m.Vertices[m.Triangles[t + 1]], m.Vertices[m.Triangles[t + 2]]};
        float cx = 0, cy = 0, cz = 0;
        for (const BchVertex& v : p) { cx += v.Position[0] / 3; cy += v.Position[1] / 3; cz += v.Position[2] / 3; }
        if (!keep(Column(cx), cy, Row(cz), p)) continue;
        p = ClipPlane(p, 0, x0, true); if (p.size() < 3) continue;
        p = ClipPlane(p, 0, x1, false); if (p.size() < 3) continue;
        p = ClipPlane(p, 2, z0, true); if (p.size() < 3) continue;
        p = ClipPlane(p, 2, z1, false); if (p.size() < 3) continue;
        const uint32_t base = (uint32_t)outVertices.size();
        outVertices.insert(outVertices.end(), p.begin(), p.end());
        for (size_t k = 1; k + 1 < p.size(); k++) outTriangles.insert(outTriangles.end(), {base, base + (uint32_t)k, base + (uint32_t)k + 1});
    }
}

// a piece of the donor: per donor mesh, vertices and triangles
struct Kit
{
    std::map<size_t, std::pair<Polygon, std::vector<uint32_t>>> Meshes;
    // texture coordinates' change per unit along x and z (per mesh), to carry the texture on when the kit is moved
    std::map<size_t, std::array<float, 4>> Gradient; // du/dx, du/dz, dv/dx, dv/dz
    bool Empty() const { for (const auto& [m, g] : Meshes) if (!g.second.empty()) return false; return true; }
};

// least squares of u and v against x (and z) over the kit's vertices of one mesh; an axis the vertices do not vary along gets 0
void FitGradient(Kit& kit)
{
    for (const auto& [m, g] : kit.Meshes)
    {
        const Polygon& v = g.first;
        std::array<float, 4> out = {0, 0, 0, 0};
        for (int axis : {0, 2})
        {
            double mean = 0, mu = 0, mv = 0;
            for (const BchVertex& p : v) { mean += p.Position[axis]; mu += p.TexCoord[0]; mv += p.TexCoord[1]; }
            if (v.empty()) continue;
            mean /= v.size(); mu /= v.size(); mv /= v.size();
            double sxx = 0, sxu = 0, sxv = 0;
            for (const BchVertex& p : v) { const double d = p.Position[axis] - mean; sxx += d * d; sxu += d * (p.TexCoord[0] - mu); sxv += d * (p.TexCoord[1] - mv); }
            if (sxx < 1) continue; // the kit does not extend along this axis
            out[axis == 0 ? 0 : 1] = (float)(sxu / sxx);
            out[axis == 0 ? 2 : 3] = (float)(sxv / sxx);
        }
        kit.Gradient[m] = out;
    }
}

// the kit moved by (dx, dz) units, scaled about (cx, cz) by (sx, sz) first, turned a quarter (x, z) -> (-z, x) about it if asked;
// the texture carried on along the move when carryTexture
void Place(std::map<size_t, BchGeometry>& geo, const Kit& kit, float dx, float dz, bool carryTexture, float cx = 0, float cz = 0,
           float sx = 1, float sz = 1, bool quarterTurn = false)
{
    for (const auto& [m, g] : kit.Meshes)
    {
        BchGeometry& out = geo[m];
        out.Mesh = m;
        const uint32_t base = (uint32_t)out.Vertices.size();
        const auto grad = kit.Gradient.count(m) ? kit.Gradient.at(m) : std::array<float, 4>{0, 0, 0, 0};
        for (BchVertex v : g.first)
        {
            float x = (v.Position[0] - cx) * sx, z = (v.Position[2] - cz) * sz;
            if (quarterTurn)
            {
                const float t = x; x = -z; z = t;
                const float n = v.Normal[0]; v.Normal[0] = -v.Normal[2]; v.Normal[2] = n;
            }
            v.Position[0] = x + cx + dx; v.Position[2] = z + cz + dz;
            if (carryTexture) { v.TexCoord[0] += grad[0] * dx + grad[1] * dz; v.TexCoord[1] += grad[2] * dx + grad[3] * dz; }
            out.Vertices.push_back(v);
        }
        for (uint32_t i : g.second) out.Triangles.push_back(base + i);
    }
}

bool Has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// the meshes the room's shell is made of (floors, walls and their caps), by their material's name
bool Shell(const std::string& name) { return Has(name, "floor") || Has(name, "wall") || Has(name, "pillar"); }

}

Bytes BuildRoom(const TownLayout& layout, const Bytes& townPiece, const Bytes& donorPiece, const RoomDonor& donor, int stairsC, int stairsR,
                const std::string& modelName, std::vector<std::string>& log)
{
    char line[256];
    auto note = [&](const std::string& s) { log.push_back("room: " + s); };
    const BinLinker donorGr = BinLinker::Read(donorPiece, "GR");
    const Bytes& donorTerrain = donorGr.Files.at(1);
    const BchModel model = Bch::Read(donorTerrain).Models.at(0);
    if (modelName.size() != model.Name.size())
        throw FormatError("room: the model's name must be " + std::to_string(model.Name.size()) + " characters as the donor's (" + model.Name + ")");
    auto meshName = [&](size_t m) { return model.Materials[model.Meshes[m].Material].Name; };

    // the room: every tile Platinum gives terrain
    std::vector<std::vector<bool>> room(N, std::vector<bool>(N, false));
    int tiles = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (!layout.TileMaterials[r][c].empty()) { room[r][c] = true; tiles++; }
    if (!tiles) throw FormatError("room: no tile has terrain");
    auto inRoom = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && room[r][c]; };

    // the shell's slices: the floor's tile, the back wall's column, the side walls' rows (their caps above and beyond them included)
    Kit floor, back, west, east, stairs;
    for (size_t m = 0; m < model.Meshes.size(); m++)
    {
        const std::string name = meshName(m);
        if (!Shell(name)) continue;
        auto& [fv, ft] = floor.Meshes[m];
        if (Has(name, "floor"))
            ClipMesh(model.Meshes[m], X((float)donor.FloorC), X(donor.FloorC + 1.0f), Z((float)donor.FloorR), Z(donor.FloorR + 1.0f),
                     [](float, float y, float, const Polygon& p) { for (const BchVertex& v : p) if (std::fabs(v.Position[1]) > 0.5f) return false; (void)y; return true; }, fv, ft);
        auto& [bv, bt] = back.Meshes[m];
        ClipMesh(model.Meshes[m], X((float)donor.BackC), X(donor.BackC + 1.0f), -1e9f, 1e9f,
                 [&](float, float, float r, const Polygon&) { return r >= donor.BackZ - 1.6f && r <= donor.BackZ + 0.06f; }, bv, bt);
        auto& [wv, wt] = west.Meshes[m];
        ClipMesh(model.Meshes[m], -1e9f, 1e9f, Z((float)donor.LeftR), Z(donor.LeftR + 1.0f),
                 [&](float c, float, float, const Polygon&) { return c >= donor.LeftX - 1.6f && c <= donor.LeftX + 0.06f; }, wv, wt);
        auto& [ev, et] = east.Meshes[m];
        ClipMesh(model.Meshes[m], -1e9f, 1e9f, Z((float)donor.RightR), Z(donor.RightR + 1.0f),
                 [&](float c, float, float, const Polygon&) { return c >= donor.RightX - 0.06f && c <= donor.RightX + 1.6f; }, ev, et);
    }
    for (Kit* k : {&floor, &back, &west, &east}) FitGradient(*k);
    if (floor.Empty() || back.Empty() || west.Empty() || east.Empty()) throw FormatError("room: the donor's floor or a wall slice is empty (RoomDonor's tiles)");

    std::map<size_t, BchGeometry> geo;

    // the stairs, on Platinum's stairs tiles (their group's box): the donor's stairs box laid with its warp column on the group's
    // first column, against the back wall (the opening going down) or with its foot on the group's last row (the stairs going up)
    float stairsBox[4] = {0, 0, 0, 0}, stairsShift[2] = {0, 0};
    bool haveStairs = false;
    {
        int c0 = N, c1 = -1, r0 = N, r1 = -1;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                for (const std::string& m : layout.TileMaterials[r][c])
                    if (m.rfind("stair", 0) == 0) { c0 = std::min(c0, c); c1 = std::max(c1, c); r0 = std::min(r0, r); r1 = std::max(r1, r); }
        if (c1 >= 0 && stairsC >= 0)
        {
            const bool up = donor.StairsBox[3] - donor.StairsR > 0.5f && donor.StairsR > donor.StairsBox[2] + 1.5f; // the warp low in the box: going up
            const float dc = c0 - (float)donor.StairsC;
            const float dr = up ? (r1 + 1) - donor.StairsBox[3] + 0.05f : r0 - (donor.StairsBox[2] + 0.1f);
            // the donor's box, cut to the room's columns: the opening down comes with 4.3 tiles of floor and railing, which ran
            // 2 columns past house 415's east wall
            int roomC0 = N, roomC1 = -1;
            for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (room[r][c]) { roomC0 = std::min(roomC0, c); roomC1 = std::max(roomC1, c); }
            stairsBox[0] = std::max(donor.StairsBox[0] + dc, (float)roomC0); stairsBox[1] = std::min(donor.StairsBox[1] + dc, roomC1 + 1.0f);
            stairsBox[2] = donor.StairsBox[2] + dr; stairsBox[3] = donor.StairsBox[3] + dr;
            for (size_t m = 0; m < model.Meshes.size(); m++)
            {
                auto& [sv, st] = stairs.Meshes[m];
                ClipMesh(model.Meshes[m], X(stairsBox[0] - dc), X(stairsBox[1] - dc), Z(donor.StairsBox[2]), Z(donor.StairsBox[3]),
                         [](float, float, float, const Polygon&) { return true; }, sv, st);
            }
            Place(geo, stairs, dc * T, dr * T, false);
            stairsShift[0] = dc; stairsShift[1] = dr;
            haveStairs = true;
            snprintf(line, sizeof line, "stairs %s on tiles (%d-%d, %d-%d), the donor's at columns %.2f-%.2f, rows %.2f-%.2f (the warp stays on (%d, %d))",
                     up ? "up" : "down", c0, c1, r0, r1, stairsBox[0], stairsBox[1], stairsBox[2], stairsBox[3], stairsC, stairsR);
            note(line);
        }
        else if (c1 >= 0) note("stairs tiles but no stairs warp: no stairs model");
    }
    // the stairs' own floor and steps down (triangles no higher than the floor), where the room's floor would cover them
    std::vector<std::array<float, 6>> stairsFloor;
    if (haveStairs)
        for (const auto& [m, g] : stairs.Meshes)
            for (size_t t = 0; t + 2 < g.second.size(); t += 3)
            {
                std::array<float, 6> tri;
                bool low = true;
                for (int k = 0; k < 3; k++)
                {
                    const BchVertex& v = g.first[g.second[t + k]];
                    if (v.Position[1] > 0.5f) low = false;
                    tri[k * 2] = Column(v.Position[0]) + stairsShift[0];
                    tri[k * 2 + 1] = Row(v.Position[2]) + stairsShift[1];
                }
                if (low) stairsFloor.push_back(tri);
            }
    auto stairsFloorAt = [&](float c, float r) {
        for (const auto& t : stairsFloor)
        {
            const float d1 = (c - t[2]) * (t[1] - t[3]) - (t[0] - t[2]) * (r - t[3]), d2 = (c - t[4]) * (t[3] - t[5]) - (t[2] - t[4]) * (r - t[5]),
                        d3 = (c - t[0]) * (t[5] - t[1]) - (t[4] - t[0]) * (r - t[1]);
            if (!(((d1 < 0) || (d2 < 0) || (d3 < 0)) && ((d1 > 0) || (d2 > 0) || (d3 > 0)))) return true;
        }
        return false;
    };
    auto underStairs = [&](float c, float r) { return haveStairs && c > stairsBox[0] && c < stairsBox[1] && r > stairsBox[2] && r < stairsBox[3]; };

    // the floor on every room tile, the walls on every edge the room ends on
    int floors = 0, walls = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c]) continue;
            if (!stairsFloorAt(c + 0.5f, r + 0.5f)) { Place(geo, floor, (c - donor.FloorC) * T, (r - donor.FloorR) * T, true); floors++; }
            if (!inRoom(c, r - 1) && !underStairs(c + 0.5f, r + 0.1f)) { Place(geo, back, (c - donor.BackC) * T, (r - donor.BackZ) * T, true); walls++; }
            if (!inRoom(c - 1, r)) { Place(geo, west, (c - donor.LeftX) * T, (r - donor.LeftR) * T, true); walls++; }
            if (!inRoom(c + 1, r)) { Place(geo, east, (c + 1 - donor.RightX) * T, (r - donor.RightR) * T, true); walls++; }
        }

    snprintf(line, sizeof line, "%d tiles, %d floor tiles, %d wall slices, no furniture (the owner: walls first), donor %s", tiles, floors, walls, model.Name.c_str());
    note(line);

    std::vector<BchGeometry> list;
    for (size_t m = 0; m < model.Meshes.size(); m++)
    {
        BchGeometry g = geo.count(m) ? std::move(geo[m]) : BchGeometry{};
        g.Mesh = m;
        if (g.Vertices.size() > 65536) throw FormatError("room: mesh " + meshName(m) + " passes 65536 vertices");
        if (g.Vertices.empty()) g.Vertices.resize(1); // a mesh that draws nothing keeps one vertex, as BuildTown's
        list.push_back(std::move(g));
    }
    BinLinker gr = BinLinker::Read(townPiece, "GR");
    gr.Files.at(1) = BchReplaceString(BchReplaceGeometry(donorTerrain, 0, list), model.Name, modelName);
    return gr.Write();
}

}
