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

// what hangs on the donor's walls (windows, curtains): never carried with the furniture beside it (t101r0102's window beside its
// bed went with the bed and stood in the middle of house 413, run rm4)
bool OnWall(const std::string& name) { return Has(name, "window") || Has(name, "curtain"); }

// a mesh's connected parts (triangles sharing vertex positions), each with its box in tiles
struct MeshPart { size_t Mesh; std::vector<uint32_t> Triangles; float C0 = 1e9, C1 = -1e9, R0 = 1e9, R1 = -1e9; };

std::vector<MeshPart> Parts(const BchModel& model, size_t mesh)
{
    const BchMesh& m = model.Meshes[mesh];
    std::map<std::tuple<long, long, long>, int> id;
    std::vector<int> up;
    std::function<int(int)> find = [&](int a) { return up[a] == a ? a : up[a] = find(up[a]); };
    std::vector<int> first;
    for (size_t t = 0; t + 2 < m.Triangles.size(); t += 3)
    {
        int f = -1;
        for (int k = 0; k < 3; k++)
        {
            const auto& q = m.Vertices[m.Triangles[t + k]].Position;
            const auto key = std::make_tuple(std::lround(q[0] * 10), std::lround(q[1] * 10), std::lround(q[2] * 10));
            auto it = id.find(key);
            if (it == id.end()) { it = id.emplace(key, (int)up.size()).first; up.push_back((int)up.size()); }
            if (f < 0) f = it->second; else up[find(it->second)] = find(f);
        }
        first.push_back(f);
    }
    std::map<int, MeshPart> parts;
    for (size_t k = 0; k < first.size(); k++)
    {
        MeshPart& p = parts[find(first[k])];
        p.Mesh = mesh;
        for (int j = 0; j < 3; j++)
        {
            const uint32_t i = m.Triangles[k * 3 + j];
            p.Triangles.push_back(i);
            p.C0 = std::min(p.C0, Column(m.Vertices[i].Position[0])); p.C1 = std::max(p.C1, Column(m.Vertices[i].Position[0]));
            p.R0 = std::min(p.R0, Row(m.Vertices[i].Position[2])); p.R1 = std::max(p.R1, Row(m.Vertices[i].Position[2]));
        }
    }
    std::vector<MeshPart> out;
    for (auto& [k, p] : parts) out.push_back(std::move(p));
    return out;
}

void AddPart(Kit& kit, const BchModel& model, const MeshPart& p)
{
    auto& [v, tri] = kit.Meshes[p.Mesh];
    std::map<uint32_t, uint32_t> remap;
    for (uint32_t i : p.Triangles)
    {
        auto it = remap.find(i);
        if (it == remap.end()) { it = remap.emplace(i, (uint32_t)v.size()).first; v.push_back(model.Meshes[p.Mesh].Vertices[i]); }
        tri.push_back(it->second);
    }
}

// what each Platinum furniture material is in ORAS (the donor's material name it starts with, in order of preference), read
// from the two games' material names: Platinum's room textures are named as ORAS's are (table01, ref01, sink01, tv01, ...)
const std::vector<std::pair<std::string, std::vector<std::string>>>& Kinds()
{
    static const std::vector<std::pair<std::string, std::vector<std::string>>> kinds = {
        {"table01", {"table01"}}, {"table_l01", {"desk01", "table01"}}, {"chair01", {"chair01", "cushion"}}, {"chair02", {"chair01", "cushion"}},
        {"chair04", {"chair01", "cushion"}}, {"cushion01", {"cushion"}}, {"ref01", {"ref01"}}, {"sink01", {"sink01"}}, {"tv01", {"tv01"}},
        {"game_h01", {"obj1"}}, {"counter_h01", {"shelf02"}}, {"wall01", {"bookshelf01", "shelf01"}}, {"shelf01", {"shelf01", "bookshelf01"}},
        {"plant01", {"plant01"}}, {"bed_h01", {"bed01"}}, {"d_mat01", {"mat01"}}, {"d_mat04", {"mat01", "mat02"}},
    };
    return kinds;
}

// a Platinum material's furniture kind ("table01_mat" -> "table01", "carpet03_1mat" -> "carpet"); empty: the room's shell
std::string KindOf(const std::string& material)
{
    if (material.rfind("carpet", 0) == 0) return "carpet";
    if (material.rfind("stair", 0) == 0) return "stairs";
    std::string stem = material;
    if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, "_mat") == 0) stem.resize(stem.size() - 4);
    for (const auto& [kind, oras] : Kinds()) if (kind == stem) return kind;
    return "";
}

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
                    if (KindOf(m) == "stairs") { c0 = std::min(c0, c); c1 = std::max(c1, c); r0 = std::min(r0, r); r1 = std::max(r1, r); }
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

    // the furniture: each group of tiles of one kind gets the donor's object of that kind
    std::vector<MeshPart> parts;
    for (size_t m = 0; m < model.Meshes.size(); m++) for (MeshPart& p : Parts(model, m)) parts.push_back(std::move(p));
    std::set<std::string> missing;
    int placed = 0;
    std::map<std::string, std::vector<std::vector<bool>>> tilesOf;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            for (const std::string& m : layout.TileMaterials[r][c])
            {
                const std::string kind = KindOf(m);
                if (kind.empty() || kind == "stairs") continue;
                auto& t = tilesOf[kind];
                if (t.empty()) t.assign(N, std::vector<bool>(N, false));
                t[r][c] = true;
            }
    for (auto& [kind, mask] : tilesOf)
    {
        // the donor's object: the first mesh named as the kind, its parts gathered into objects (boxes within 0.15 tile), the largest
        std::vector<std::string> oras = {"carpet01"};
        for (const auto& [k, o] : Kinds()) if (k == kind) oras = o;
        int mesh = -1;
        for (const std::string& want : oras)
        {
            for (size_t m = 0; m < model.Meshes.size() && mesh < 0; m++) if (meshName(m).rfind(want, 0) == 0) mesh = (int)m;
            if (mesh >= 0) break;
        }
        if (mesh < 0) { missing.insert(kind); continue; }
        std::vector<const MeshPart*> own;
        for (const MeshPart& p : parts) if ((int)p.Mesh == mesh) own.push_back(&p);
        std::vector<std::vector<const MeshPart*>> objects;
        for (const MeshPart* p : own)
        {
            std::vector<size_t> near;
            for (size_t o = 0; o < objects.size(); o++)
                for (const MeshPart* q : objects[o])
                    if (p->C0 <= q->C1 + 0.15f && q->C0 <= p->C1 + 0.15f && p->R0 <= q->R1 + 0.15f && q->R0 <= p->R1 + 0.15f) { near.push_back(o); break; }
            if (near.empty()) { objects.push_back({p}); continue; }
            objects[near[0]].push_back(p);
            for (size_t k = near.size(); k-- > 1;) { auto& into = objects[near[0]]; into.insert(into.end(), objects[near[k]].begin(), objects[near[k]].end()); objects.erase(objects.begin() + (long)near[k]); }
        }
        auto size = [](const std::vector<const MeshPart*>& o) { size_t n = 0; for (const MeshPart* p : o) n += p->Triangles.size(); return n; };
        const auto& object = *std::max_element(objects.begin(), objects.end(), [&](const auto& a, const auto& b) { return size(a) < size(b); });
        float oc0 = 1e9, oc1 = -1e9, or0 = 1e9, or1 = -1e9;
        for (const MeshPart* p : object) { oc0 = std::min(oc0, p->C0); oc1 = std::max(oc1, p->C1); or0 = std::min(or0, p->R0); or1 = std::max(or1, p->R1); }
        // what stands on it or lies under it: other meshes' parts inside its box (0.2 tile around), the shell's and other kinds' aside
        Kit kit;
        for (const MeshPart* p : object) AddPart(kit, model, *p);
        for (const MeshPart& p : parts)
        {
            if ((int)p.Mesh == mesh || Shell(meshName(p.Mesh)) || OnWall(meshName(p.Mesh))) continue;
            bool otherKind = false;
            for (const auto& [k, o] : Kinds()) for (const std::string& w : o) if (meshName(p.Mesh).rfind(w, 0) == 0) otherKind = true;
            if (meshName(p.Mesh).rfind("carpet", 0) == 0) otherKind = true;
            if (otherKind) continue;
            if (p.C0 >= oc0 - 0.2f && p.C1 <= oc1 + 0.2f && p.R0 >= or0 - 0.2f && p.R1 <= or1 + 0.2f) AddPart(kit, model, p);
        }

        // each group of the kind's tiles (4-connected)
        std::vector<std::vector<bool>> seen(N, std::vector<bool>(N, false));
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
            {
                if (!mask[r][c] || seen[r][c]) continue;
                int c0 = c, c1 = c, r0 = r, r1 = r;
                std::vector<std::pair<int, int>> todo = {{c, r}};
                while (!todo.empty())
                {
                    const auto [x, y] = todo.back();
                    todo.pop_back();
                    if (x < 0 || y < 0 || x >= N || y >= N || !mask[y][x] || seen[y][x]) continue;
                    seen[y][x] = true;
                    c0 = std::min(c0, x); c1 = std::max(c1, x); r0 = std::min(r0, y); r1 = std::max(r1, y);
                    todo.insert(todo.end(), {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}});
                }
                const float gw = (float)(c1 - c0 + 1), gh = (float)(r1 - r0 + 1), gc = c0 + gw / 2, gr = r0 + gh / 2;
                const float ow = oc1 - oc0, oh = or1 - or0, ocx = (oc0 + oc1) / 2, ocz = (or0 + or1) / 2;
                bool against = true; // the group's top row against the back wall
                for (int x = c0; x <= c1; x++) if (inRoom(x, r0 - 1)) against = false;
                const bool flat = kind == "carpet" || kind.rfind("d_mat", 0) == 0;
                const bool turn = flat && kind != "carpet" && (gw < gh) != (ow < oh) && std::fabs(gw - gh) > 0.5f;
                float sx = 1, sz = 1;
                if (kind == "carpet") { sx = gw / std::max(ow, 0.1f); sz = gh / std::max(oh, 0.1f); }
                const float depth = turn ? ow : oh;
                const float tz = against && !flat ? r0 + depth / 2 : gr;
                Place(geo, kit, X(gc) - X(ocx), Z(tz) - Z(ocz), false, X(ocx), Z(ocz), sx, sz, turn);
                placed++;
                snprintf(line, sizeof line, "%s on tiles (%d-%d, %d-%d): %s (donor columns %.2f-%.2f, rows %.2f-%.2f)%s%s", kind.c_str(), c0, c1, r0, r1,
                         meshName((size_t)mesh).c_str(), oc0, oc1, or0, or1, against && !flat ? ", against the back wall" : "", turn ? ", turned" : "");
                note(line);
            }
    }
    for (const std::string& k : missing) note("no " + k + " in the donor room (" + model.Name + "): left as floor");
    snprintf(line, sizeof line, "%d tiles, %d floor tiles, %d wall slices, %d objects, donor %s", tiles, floors, walls, placed, model.Name.c_str());
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
