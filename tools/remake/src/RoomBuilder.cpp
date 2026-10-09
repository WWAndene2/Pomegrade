#include "RoomBuilder.h"

#include "Bch.h"
#include "BchWriter.h"
#include "BinLinker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>

namespace remake
{

// t101r0101 (a/0/3/9 member 506), its textures in area pack 112
const RoomDonor GroundFloorRoom = {506, 112};

namespace
{

constexpr int N = TownTiles;
constexpr float T = 18; // units a tile
float X(float column) { return (column - 20) * T; }
float Z(float row) { return (row - 20) * T; }
float Column(float x) { return x / T + 20; }
float Row(float z) { return z / T + 20; }

// ORAS's measures, read on t101r0101's meshes (scratch scan of its vertices, 9 October), units (a tile 18) unless said:
// - the wall (material wall_01): 54 high, its texture's v at heights 0, 2, 15.5, 48.8, 49.9, 54: 0, 0.045, 0.342, 0.910,
//   0.928, 1 (skirting, wallpaper, moulding); u 0.27 a tile along the back wall, 0.183 along a side wall; vertex colour
//   (1, 0.96, 0.92) on the back wall and (0.70, 0.61, 0.52) on a side wall up to 48.8, from 49.9 the moulding's (0.75, 0.66,
//   0.58) and (0.58, 0.49, 0.40); a side wall's end, where the room's south edge is, 0.31 tile thick, colour (0.56, 0.56, 0.56)
// - the cap (wall_top): its face on the wall's plane from 54 to 64.7, v 5.004-5.214 (54-57) then 5.399-5.639 (57-64.7), u 0.316
//   a tile, colour 0.92 (back) and 0.72 (side); its top at 64.7, one tile deep beyond the wall: a light band 0.36 tile deep (colour
//   1 at the wall, 0.8 at its far side; v 6.693 to 6.803, u 0.314 a tile), the rest black
// - the floor (floor01_r): u = 0.3036 column - 3.646, v = -0.4547 row + 7.033; colour (0.61, 0.44, 0.25) on a wall's foot,
//   (0.91, 0.79, 0.55) 0.27 tile from it, white from 1.06 tile
// - the south edge (floor03_r): a lip from 0 down to -9.1, colour 0.5 down to 0, u 0.611 a tile, v 4.435 to 4.092
// - the way out: the floor one tile further south, 3 tiles wide (columns 15-18 beyond row 18), its lip around it, the mat (mat01)
//   centred at column 16.49, row 17.68 and the light (ent_light01_a) over columns 15-18, rows 16.95-19
// - the window: an opening in the back wall from column 7.13 to 9.88 (centre 8.505), height 19.8 to 51.3, recessed to row 5.48
//   (the wall on row 6), with its sill (floor03_r), recess sides (wall_01), frame (window02_r) and glass (window01_gr)
constexpr float WallBands[] = {0, 2, 15.5f, 48.8f, 49.9f, 54}, WallV[] = {0, 0.045f, 0.342f, 0.910f, 0.928f, 1};
constexpr float WallTop = 54, CapTop = 64.7f, Lip = -9.1f, SideEnd = 0.31f;
constexpr float WindowCentre = 8.505f, WindowHalf = 1.375f, WindowBottom = 19.8f, WindowTop = 51.3f, DonorBackWall = 6;
constexpr float DonorFront = 18, DoorCentre = 16.5f;

float WallVAt(float y)
{
    for (size_t k = 1; k < std::size(WallBands); k++)
        if (y <= WallBands[k]) return WallV[k - 1] + (WallV[k] - WallV[k - 1]) * (y - WallBands[k - 1]) / (WallBands[k] - WallBands[k - 1]);
    return 1;
}

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

// a piece of the donor (per donor mesh, vertices and triangles), to be moved into the room as it is
using Kit = std::map<size_t, std::pair<Polygon, std::vector<uint32_t>>>;

// the mesh's triangles whose centre lies in a box of tiles (columns, rows; heights in units), clipped to its columns and rows
void Cut(const BchMesh& m, float c0, float c1, float r0, float r1, float y0, float y1, Polygon& outVertices, std::vector<uint32_t>& outTriangles)
{
    for (size_t t = 0; t + 2 < m.Triangles.size(); t += 3)
    {
        Polygon p = {m.Vertices[m.Triangles[t]], m.Vertices[m.Triangles[t + 1]], m.Vertices[m.Triangles[t + 2]]};
        float cx = 0, cy = 0, cz = 0;
        for (const BchVertex& v : p) { cx += v.Position[0] / 3; cy += v.Position[1] / 3; cz += v.Position[2] / 3; }
        if (Column(cx) < c0 || Column(cx) > c1 || Row(cz) < r0 || Row(cz) > r1 || cy < y0 || cy > y1) continue;
        p = ClipPlane(p, 0, X(c0), true); if (p.size() < 3) continue;
        p = ClipPlane(p, 0, X(c1), false); if (p.size() < 3) continue;
        p = ClipPlane(p, 2, Z(r0), true); if (p.size() < 3) continue;
        p = ClipPlane(p, 2, Z(r1), false); if (p.size() < 3) continue;
        const uint32_t base = (uint32_t)outVertices.size();
        outVertices.insert(outVertices.end(), p.begin(), p.end());
        for (size_t k = 1; k + 1 < p.size(); k++) outTriangles.insert(outTriangles.end(), {base, base + (uint32_t)k, base + (uint32_t)k + 1});
    }
}

void Place(std::map<size_t, BchGeometry>& geo, const Kit& kit, float dColumns, float dRows)
{
    for (const auto& [m, g] : kit)
    {
        BchGeometry& out = geo[m];
        const uint32_t base = (uint32_t)out.Vertices.size();
        for (BchVertex v : g.first) { v.Position[0] += dColumns * T; v.Position[2] += dRows * T; out.Vertices.push_back(v); }
        for (uint32_t i : g.second) out.Triangles.push_back(base + i);
    }
}

// a corner of a computed rectangle: tiles (column, row) and a height in units, its texture coordinates and colour
struct Corner { float C, Y, R, U, V; std::array<float, 3> Colour; };

// one rectangle (a, b, c, d around it), facing `normal`; the donor's winding (front faces) by `windingSign`
void Quad(BchGeometry& g, const Corner (&q)[4], const std::array<float, 3>& normal, float windingSign)
{
    const uint32_t base = (uint32_t)g.Vertices.size();
    for (const Corner& k : q)
    {
        BchVertex v;
        v.Position[0] = X(k.C); v.Position[1] = k.Y; v.Position[2] = Z(k.R);
        v.Normal[0] = normal[0]; v.Normal[1] = normal[1]; v.Normal[2] = normal[2];
        v.TexCoord[0] = k.U; v.TexCoord[1] = k.V;
        v.Colour[0] = k.Colour[0]; v.Colour[1] = k.Colour[1]; v.Colour[2] = k.Colour[2]; v.Colour[3] = 1;
        g.Vertices.push_back(v);
    }
    // the first triangle's facing against the normal decides the order
    const BchVertex *a = &g.Vertices[base], *b = &g.Vertices[base + 1], *c = &g.Vertices[base + 2];
    float e1[3], e2[3];
    for (int k = 0; k < 3; k++) { e1[k] = b->Position[k] - a->Position[k]; e2[k] = c->Position[k] - a->Position[k]; }
    const float facing = (e1[1] * e2[2] - e1[2] * e2[1]) * normal[0] + (e1[2] * e2[0] - e1[0] * e2[2]) * normal[1] + (e1[0] * e2[1] - e1[1] * e2[0]) * normal[2];
    if (facing * windingSign >= 0) g.Triangles.insert(g.Triangles.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    else g.Triangles.insert(g.Triangles.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

std::array<float, 3> Mix(const std::array<float, 3>& a, const std::array<float, 3>& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}

}

Bytes BuildRoom(const TownLayout& layout, const Bytes& townPiece, const Bytes& donorPiece, const RoomDonor& donor, const std::string& modelName,
                std::vector<std::string>& log)
{
    char line[256];
    auto note = [&](const std::string& s) { log.push_back("room: " + s); };
    const BinLinker donorGr = BinLinker::Read(donorPiece, "GR");
    const Bytes& donorTerrain = donorGr.Files.at(1);
    const BchModel model = Bch::Read(donorTerrain).Models.at(0);
    if (modelName.size() != model.Name.size())
        throw FormatError("room: the model's name must be " + std::to_string(model.Name.size()) + " characters as the donor's (" + model.Name + ")");
    auto meshNamed = [&](const std::string& name) {
        for (size_t m = 0; m < model.Meshes.size(); m++) if (model.Materials[model.Meshes[m].Material].Name == name) return m;
        throw FormatError("room: the donor " + model.Name + " has no mesh of material " + name);
    };
    const size_t floorMesh = meshNamed("floor01_r"), wallMesh = meshNamed("wall_01"), capMesh = meshNamed("wall_top"), lipMesh = meshNamed("floor03_r");
    // the donor's front faces: its floor's first triangle, which faces up
    float winding = 1;
    {
        const BchMesh& f = model.Meshes[floorMesh];
        const auto &a = f.Vertices[f.Triangles[0]].Position, &b = f.Vertices[f.Triangles[1]].Position, &c = f.Vertices[f.Triangles[2]].Position;
        winding = ((c[0] - a[0]) * (b[2] - a[2]) - (b[0] - a[0]) * (c[2] - a[2])) >= 0 ? 1.0f : -1.0f;
    }

    // the room: every tile Platinum gives terrain
    std::vector<std::vector<bool>> room(N, std::vector<bool>(N, false));
    int tiles = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (!layout.TileMaterials[r][c].empty()) { room[r][c] = true; tiles++; }
    if (!tiles) throw FormatError("room: no tile has terrain");
    auto inRoom = [&](int c, int r) { return c >= 0 && r >= 0 && c < N && r < N && room[r][c]; };

    std::map<size_t, BchGeometry> geo;
    for (size_t m = 0; m < model.Meshes.size(); m++) geo[m].Mesh = m;

    // the stairs: none drawn yet. The donor's stairs, cut out of its room, came with cut walls and caps (run wl2) and rise north,
    // where Platinum's stairs (stair_u01, stair_d01) are side stairs: the warp alone, until the owner chooses the stairs' model
    for (const TownObject& o : layout.Objects)
        if (o.Name.rfind("stair", 0) == 0) note("stairs (" + o.Name + ") left without a model: the warp alone");
    auto inStairs = [](float, float) { return false; };

    // the windows: Platinum's window01 models, each on the back wall it stands on (its row just north of the wall's line)
    struct Window { float C; int Row; };
    std::vector<Window> windows;
    for (const TownObject& o : layout.Objects)
    {
        if (o.Name.rfind("window", 0) != 0) continue;
        const int r = (int)std::lround(o.Row + 0.5f);
        bool fits = true;
        for (int c = (int)std::floor(o.Column - WindowHalf); c < (int)std::ceil(o.Column + WindowHalf); c++)
            if (!inRoom(c, r) || inRoom(c, r - 1)) fits = false;
        snprintf(line, sizeof line, "window (%s) at column %.2f on the back wall of row %d%s", o.Name.c_str(), o.Column, r, fits ? "" : ": the wall is not straight there, left out");
        note(line);
        if (fits) windows.push_back({o.Column, r});
    }

    // the walls: on each room tile's north, west and east edge where the room ends
    int wallEdges = 0;
    const std::array<float, 3> backWall = {1.0f, 0.96f, 0.92f}, backTrim = {0.75f, 0.66f, 0.58f}, sideWall = {0.70f, 0.61f, 0.52f}, sideTrim = {0.58f, 0.49f, 0.40f};
    auto wallColour = [&](float y, bool back) { return y >= 49.9f ? (back ? backTrim : sideTrim) : (back ? backWall : sideWall); };
    // a band of wall between two heights and two points along its line: (c0, r0) to (c1, r1), facing `normal`
    auto wallPiece = [&](float c0, float r0, float c1, float r1, float y0, float y1, bool back, const std::array<float, 3>& normal, float u0, float u1) {
        const Corner q[4] = {{c0, y0, r0, u0, WallVAt(y0), wallColour(y0, back)}, {c1, y0, r1, u1, WallVAt(y0), wallColour(y0, back)},
                             {c1, y1, r1, u1, WallVAt(y1), wallColour(y1, back)}, {c0, y1, r0, u0, WallVAt(y1), wallColour(y1, back)}};
        Quad(geo[wallMesh], q, normal, winding);
    };
    auto wallBands = [&](float c0, float r0, float c1, float r1, float yFrom, float yTo, bool back, const std::array<float, 3>& normal, float u0, float u1) {
        std::vector<float> ys = {yFrom};
        for (float b : WallBands) if (b > yFrom && b < yTo) ys.push_back(b);
        ys.push_back(yTo);
        for (size_t k = 0; k + 1 < ys.size(); k++) wallPiece(c0, r0, c1, r1, ys[k], ys[k + 1], back, normal, u0, u1);
    };
    auto capFace = [&](float c0, float r0, float c1, float r1, const std::array<float, 3>& normal, float along0, float along1, float colour) {
        const std::array<float, 3> k = {colour, colour, colour};
        const Corner a[4] = {{c0, WallTop, r0, 0.316f * along0, 5.004f, k}, {c1, WallTop, r1, 0.316f * along1, 5.004f, k},
                             {c1, 57, r1, 0.316f * along1, 5.214f, k}, {c0, 57, r0, 0.316f * along0, 5.214f, k}};
        Quad(geo[capMesh], a, normal, winding);
        const Corner b[4] = {{c0, 57, r0, 0.316f * along0, 5.399f, k}, {c1, 57, r1, 0.316f * along1, 5.399f, k},
                             {c1, CapTop, r1, 0.316f * along1, 5.639f, k}, {c0, CapTop, r0, 0.316f * along0, 5.639f, k}};
        Quad(geo[capMesh], b, normal, winding);
    };
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c]) continue;
            if (!inRoom(c, r - 1) && !inStairs(c + 0.5f, r))
            {
                const std::array<float, 3> n = {0, 0, 1};
                const float u0 = 0.27f * c, u1 = 0.27f * (c + 1);
                // a window's opening: the wall left whole beside it, and below and above it
                float h0 = c + 1.0f, h1 = c + 1.0f;
                for (const Window& w : windows)
                    if (w.Row == r && w.C + WindowHalf > c && w.C - WindowHalf < c + 1) { h0 = std::max((float)c, w.C - WindowHalf); h1 = std::min(c + 1.0f, w.C + WindowHalf); }
                if (h0 >= c + 1.0f) wallBands((float)c, (float)r, c + 1.0f, (float)r, 0, WallTop, true, n, u0, u1);
                else
                {
                    const float uh0 = 0.27f * h0, uh1 = 0.27f * h1;
                    if (h0 > c) wallBands((float)c, (float)r, h0, (float)r, 0, WallTop, true, n, u0, uh0);
                    if (h1 < c + 1) wallBands(h1, (float)r, c + 1.0f, (float)r, 0, WallTop, true, n, uh1, u1);
                    wallBands(h0, (float)r, h1, (float)r, 0, WindowBottom, true, n, uh0, uh1);
                    wallBands(h0, (float)r, h1, (float)r, WindowTop, WallTop, true, n, uh0, uh1);
                }
                capFace((float)c, (float)r, c + 1.0f, (float)r, n, (float)c, c + 1.0f, 0.92f);
                wallEdges++;
            }
            if (!inRoom(c - 1, r))
            {
                const std::array<float, 3> n = {1, 0, 0};
                wallBands((float)c, (float)r, (float)c, r + 1.0f, 0, WallTop, false, n, -0.183f * r, -0.183f * (r + 1));
                capFace((float)c, (float)r, (float)c, r + 1.0f, n, (float)r, r + 1.0f, 0.72f);
                wallEdges++;
            }
            if (!inRoom(c + 1, r))
            {
                const std::array<float, 3> n = {-1, 0, 0};
                wallBands(c + 1.0f, (float)r, c + 1.0f, r + 1.0f, 0, WallTop, false, n, 0.183f * r, 0.183f * (r + 1));
                capFace(c + 1.0f, (float)r, c + 1.0f, r + 1.0f, n, (float)r, r + 1.0f, 0.72f);
                wallEdges++;
            }
        }

    // the walls' ends at the room's south edge: the wall 0.31 tile thick, the cap one tile
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c] || inRoom(c, r + 1)) continue;
            const std::array<float, 3> n = {0, 0, 1}, end = {0.56f, 0.56f, 0.56f}, black = {0, 0, 0};
            for (int side : {-1, 1})
            {
                if (inRoom(c + side, r)) continue;
                const float edge = side < 0 ? (float)c : c + 1.0f, out = edge + side * SideEnd, capOut = edge + side;
                const float a = std::min(edge, out), b = std::max(edge, out), ca = std::min(edge, capOut), cb = std::max(edge, capOut);
                const Corner wq[4] = {{a, 0, r + 1.0f, 0.955f, WallVAt(0), end}, {b, 0, r + 1.0f, 1.0f, WallVAt(0), end},
                                      {b, WallTop, r + 1.0f, 1.0f, 1, end}, {a, WallTop, r + 1.0f, 0.955f, 1, end}};
                Quad(geo[wallMesh], wq, n, winding);
                const Corner cq[4] = {{ca, WallTop, r + 1.0f, 3.08f, 6.95f, black}, {cb, WallTop, r + 1.0f, 3.08f, 6.95f, black},
                                      {cb, CapTop, r + 1.0f, 3.08f, 6.95f, black}, {ca, CapTop, r + 1.0f, 3.08f, 6.95f, black}};
                Quad(geo[capMesh], cq, n, winding);
            }
        }

    // the cap's top: on every tile beyond a wall (north, west, east of the room and the corners between), light along the wall
    int capTiles = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (inRoom(c, r)) continue;
            const bool southRoom = inRoom(c, r + 1), westRoom = inRoom(c - 1, r), eastRoom = inRoom(c + 1, r);
            const bool corner = inRoom(c - 1, r + 1) || inRoom(c + 1, r + 1);
            if (!southRoom && !westRoom && !eastRoom && !corner) continue;
            if (inStairs(c + 0.5f, r + 0.5f)) continue;
            capTiles++;
            // the tile cut at 0.36 from each side that runs along a wall
            std::vector<float> xs = {0, 1}, zs = {0, 1};
            if (westRoom) xs.push_back(0.36f);
            if (eastRoom) xs.push_back(0.64f);
            if (southRoom) zs.push_back(0.64f);
            std::sort(xs.begin(), xs.end()); std::sort(zs.begin(), zs.end());
            auto light = [&](float x, float z) {
                float d = 1e9f;
                if (westRoom) d = std::min(d, x);
                if (eastRoom) d = std::min(d, 1 - x);
                if (southRoom) d = std::min(d, 1 - z);
                return d;
            };
            for (size_t i = 0; i + 1 < xs.size(); i++)
                for (size_t j = 0; j + 1 < zs.size(); j++)
                {
                    const float x0 = xs[i], x1 = xs[i + 1], z0 = zs[j], z1 = zs[j + 1];
                    const bool lit = light((x0 + x1) / 2, (z0 + z1) / 2) < 0.36f;
                    auto corner = [&](float x, float z) {
                        const float d = light(x, z);
                        const std::array<float, 3> k = lit ? Mix({1, 1, 1}, {0.8f, 0.79f, 0.79f}, d / 0.36f) : std::array<float, 3>{0, 0, 0};
                        const float along = southRoom ? c + x : r + z;
                        return Corner{c + x, CapTop, r + z, lit ? 0.314f * along : 3.08f, lit ? 6.693f + 0.11f * std::min(d / 0.36f, 1.0f) : 6.95f, k};
                    };
                    const Corner q[4] = {corner(x0, z0), corner(x1, z0), corner(x1, z1), corner(x0, z1)};
                    Quad(geo[capMesh], q, {0, 1, 0}, winding);
                }
        }

    // the floor: each room tile cut at 0.27 from the walls it runs along, darker towards them
    std::vector<std::array<float, 4>> wallLines; // c0, r0, c1, r1 of every wall's foot
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c]) continue;
            if (!inRoom(c, r - 1)) wallLines.push_back({(float)c, (float)r, c + 1.0f, (float)r});
            if (!inRoom(c - 1, r)) wallLines.push_back({(float)c, (float)r, (float)c, r + 1.0f});
            if (!inRoom(c + 1, r)) wallLines.push_back({c + 1.0f, (float)r, c + 1.0f, r + 1.0f});
        }
    auto wallDistance = [&](float c, float r) {
        float d = 1e9f;
        for (const auto& w : wallLines)
        {
            const float cc = std::clamp(c, w[0], w[2]), rr = std::clamp(r, w[1], w[3]);
            d = std::min(d, std::hypot(c - cc, r - rr));
        }
        return d;
    };
    auto floorColour = [&](float d) {
        const std::array<float, 3> foot = {0.61f, 0.44f, 0.25f}, near = {0.91f, 0.79f, 0.55f}, white = {1, 1, 1};
        return d <= 0.27f ? Mix(foot, near, d / 0.27f) : Mix(near, white, (d - 0.27f) / (1.06f - 0.27f));
    };
    auto floorQuad = [&](float c0, float r0, float c1, float r1) {
        auto k = [&](float c, float r) { return Corner{c, 0, r, 0.3036f * c - 3.646f, -0.4547f * r + 7.033f, floorColour(wallDistance(c, r))}; };
        const Corner q[4] = {k(c0, r0), k(c1, r0), k(c1, r1), k(c0, r1)};
        Quad(geo[floorMesh], q, {0, 1, 0}, winding);
    };
    int floorTiles = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c] || inStairs(c + 0.5f, r + 0.5f)) continue;
            std::vector<float> xs = {0, 1}, zs = {0, 1};
            if (!inRoom(c - 1, r)) xs.push_back(0.27f);
            if (!inRoom(c + 1, r)) xs.push_back(0.73f);
            if (!inRoom(c, r - 1)) zs.push_back(0.27f);
            std::sort(xs.begin(), xs.end()); std::sort(zs.begin(), zs.end());
            for (size_t i = 0; i + 1 < xs.size(); i++)
                for (size_t j = 0; j + 1 < zs.size(); j++) floorQuad(c + xs[i], r + zs[j], c + xs[i + 1], r + zs[j + 1]);
            floorTiles++;
        }

    // the way out: Platinum's exit mat (building d_mat01) gets ORAS's way out, the floor one tile further south 3 tiles wide
    std::vector<std::pair<float, int>> doors; // centre column, the room's last row there
    for (const TownObject& o : layout.Objects)
        if (o.Name.rfind("d_mat01", 0) == 0)
        {
            const int c = (int)std::floor(o.Column), r = (int)std::floor(o.Row);
            if (inRoom(c, r) && !inRoom(c, r + 1)) doors.push_back({o.Column, r});
            else note("exit mat not on the room's south edge: no way out drawn there");
        }
    auto inDoor = [&](float c, int r) { for (const auto& [dc, dr] : doors) if (r == dr + 1 && c > dc - 1.5f && c < dc + 1.5f) return true; return false; };
    for (const auto& [dc, dr] : doors)
    {
        const float c0 = dc - 1.5f, c1 = dc + 1.5f, r0 = dr + 1.0f, r1 = dr + 2.0f;
        floorQuad(c0, r0, c1, r1);
        // its lip, south, west and east
        const std::array<float, 3> top = {0.5f, 0.5f, 0.5f}, bottom = {0, 0, 0};
        const Corner s[4] = {{c0, 0, r1, 0.611f * c0, 4.435f, top}, {c1, 0, r1, 0.611f * c1, 4.435f, top}, {c1, Lip, r1, 0.611f * c1, 4.092f, bottom}, {c0, Lip, r1, 0.611f * c0, 4.092f, bottom}};
        Quad(geo[lipMesh], s, {0, 0, 1}, winding);
        const Corner w[4] = {{c0, 0, r0, 0.611f * r0, 4.435f, top}, {c0, 0, r1, 0.611f * r1, 4.435f, top}, {c0, Lip, r1, 0.611f * r1, 4.092f, bottom}, {c0, Lip, r0, 0.611f * r0, 4.092f, bottom}};
        Quad(geo[lipMesh], w, {-1, 0, 0}, winding);
        const Corner e[4] = {{c1, 0, r0, 0.611f * r0, 4.435f, top}, {c1, 0, r1, 0.611f * r1, 4.435f, top}, {c1, Lip, r1, 0.611f * r1, 4.092f, bottom}, {c1, Lip, r0, 0.611f * r0, 4.092f, bottom}};
        Quad(geo[lipMesh], e, {1, 0, 0}, winding);
        // ORAS's mat and light, at the same place against the way out as in t101r0101
        Kit kit;
        for (const char* name : {"mat01", "ent_light01_a"})
        {
            const size_t m = meshNamed(name);
            Cut(model.Meshes[m], DoorCentre - 2, DoorCentre + 2, DonorFront - 2, DonorFront + 1.5f, -1e9f, 1e9f, kit[m].first, kit[m].second);
        }
        Place(geo, kit, dc - DoorCentre, (dr + 1) - DonorFront);
        snprintf(line, sizeof line, "way out at column %.2f, beyond row %d", dc, dr);
        note(line);
    }

    // the south edge's lip, where there is no way out
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
        {
            if (!room[r][c] || inRoom(c, r + 1) || inDoor(c + 0.5f, r + 1)) continue;
            const std::array<float, 3> top = {0.5f, 0.5f, 0.5f}, bottom = {0, 0, 0};
            const Corner q[4] = {{(float)c, 0, r + 1.0f, 0.611f * c, 4.435f, top}, {c + 1.0f, 0, r + 1.0f, 0.611f * (c + 1), 4.435f, top},
                                 {c + 1.0f, Lip, r + 1.0f, 0.611f * (c + 1), 4.092f, bottom}, {(float)c, Lip, r + 1.0f, 0.611f * c, 4.092f, bottom}};
            Quad(geo[lipMesh], q, {0, 0, 1}, winding);
        }

    // each window: ORAS's, its recess, sill, frame and glass, in the opening left in the wall
    for (const Window& w : windows)
    {
        Kit kit;
        for (const char* name : {"window02_r", "window01_gr", "wall_01", "floor03_r"})
        {
            const size_t m = meshNamed(name);
            Cut(model.Meshes[m], WindowCentre - WindowHalf - 0.05f, WindowCentre + WindowHalf + 0.05f, DonorBackWall - 0.6f, DonorBackWall - 0.005f,
                WindowBottom - 1.5f, WindowTop + 1.5f, kit[m].first, kit[m].second);
        }
        Place(geo, kit, w.C - WindowCentre, w.Row - DonorBackWall);
    }

    snprintf(line, sizeof line, "%d tiles: %d floor tiles, %d wall edges, %d cap tiles, %zu window(s), %zu way(s) out; no furniture (the owner: walls first)",
             tiles, floorTiles, wallEdges, capTiles, windows.size(), doors.size());
    note(line);

    std::vector<BchGeometry> list;
    for (size_t m = 0; m < model.Meshes.size(); m++)
    {
        BchGeometry g = std::move(geo[m]);
        g.Mesh = m;
        if (g.Vertices.size() > 65536) throw FormatError("room: mesh " + model.Materials[model.Meshes[m].Material].Name + " passes 65536 vertices");
        if (g.Vertices.empty()) g.Vertices.resize(1); // a mesh that draws nothing keeps one vertex, as BuildTown's
        list.push_back(std::move(g));
    }
    BinLinker gr = BinLinker::Read(townPiece, "GR");
    gr.Files.at(1) = BchReplaceString(BchReplaceGeometry(donorTerrain, 0, list), model.Name, modelName);
    return gr.Write();
}

}
