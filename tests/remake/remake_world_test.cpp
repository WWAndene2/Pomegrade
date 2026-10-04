// The remake tooling's world mapping: a Platinum-style map matrix (2x2,
// one empty cell, with headers and height layers) and three land data
// files built here to their documented layout, assembled into the world's
// JSON, collision overview and glTF scene. As with the other remake tests,
// the layouts are only checked for being read back consistently: they still
// have to be checked on the real game's files.
#include "LandData.h"
#include "MapMatrix.h"
#include "Narc.h"
#include "Png.h"
#include "WorldMap.h"
#include "synthetic_files.h"

#include <cstdio>
#include <string>

using namespace remake;
using namespace synthetic;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

// a land data file: tile (0,0) solid, tile (1,0) behaviour 2 (tall grass),
// one building (model 0) at x 1.5, z -2, and the given terrain model
static Bytes Land(const Bytes& model)
{
    Bytes perm(LandTiles * LandTiles * 2, 0);
    perm[1] = 0x80;
    perm[2] = 2;
    Bytes build(0x30, 0);
    Put32(build, 4, 0x18000);            // x = 1.5
    Put32(build, 12, (uint32_t)-0x20000); // z = -2
    const Bytes heights = {'B', 'D', 'H', 'C'};
    Bytes d(16, 0);
    Put32(d, 0, (uint32_t)perm.size()); Put32(d, 4, (uint32_t)build.size());
    Put32(d, 8, (uint32_t)model.size()); Put32(d, 12, (uint32_t)heights.size());
    for (const Bytes* b : std::initializer_list<const Bytes*>{&perm, &build, &model, &heights}) d.insert(d.end(), b->begin(), b->end());
    return d;
}

int main()
{
    // matrix "w": 2x2, headers 10..13, height layers 0..3, land data {0, 1, none, 2}
    Bytes mx = {2, 2, 1, 1, 1, 'w'};
    for (int i = 0; i < 4; i++) Push16(mx, (uint16_t)(10 + i));
    for (int i = 0; i < 4; i++) mx.push_back((uint8_t)i);
    for (uint16_t id : {0, 1, 0xFFFF, 2}) Push16(mx, id);
    const MapMatrix m = MapMatrix::Read(mx);
    check(m.Name == "w" && m.Width == 2 && m.Height == 2 && m.Headers[3] == 13 && m.Heights[2] == 2 &&
          m.LandData == std::vector<int>({0, 1, -1, 2}), "map matrix: name, size, headers, heights, land data, empty cell");
    bool refused = false;
    try { Bytes cut(mx.begin(), mx.end() - 1); MapMatrix::Read(cut); } catch (const FormatError&) { refused = true; }
    check(refused, "map matrix: a cut file refused");
    refused = false;
    try { Bytes longer = mx; longer.push_back(0); MapMatrix::Read(longer); } catch (const FormatError&) { refused = true; }
    check(refused, "map matrix: bytes left over refused");

    const Bytes terrain = QuadNsbmd({});
    const LandData l = LandData::Read(Land(terrain));
    check(l.Permissions.size() == 1024 && LandData::Solid(l.Permissions[0]) && !LandData::Solid(l.Permissions[1]) &&
          (l.Permissions[1] & 0xFF) == 2 && l.Buildings.size() == 1 && l.Buildings[0].Position[0] == 1.5f &&
          l.Buildings[0].Position[2] == -2.0f && l.Model == terrain && l.Heights.size() == 4,
          "land data: permissions, building position, terrain model, heights");

    // land data 1 is broken (permissions of the wrong size): reported, the rest still built
    Bytes broken = Land(terrain); Put32(broken, 0, 4);
    const Narc lands(MakeNarc({Land(terrain), broken, Land(terrain)}));
    const WorldMap w(m, lands);
    check(w.Cells[0] && !w.Cells[1] && !w.Cells[2] && w.Cells[3] && w.Errors.size() == 1, "world: cells read, the broken one reported");

    const std::string json = w.Json();
    WriteFile("world.json", Bytes(json.begin(), json.end()));
    check(json.find("\"x\": 1, \"y\": 1, \"landData\": 2, \"header\": 13, \"heightLayer\": 3, \"solidTiles\": 1") != std::string::npos &&
          json.find("\"behaviours\": {\"0\": 1023, \"2\": 1}") != std::string::npos &&
          json.find("\"position\": [1.5, 0, -2]") != std::string::npos && json.find("\"error\": true") != std::string::npos,
          "world JSON: cells, permissions summary, buildings, the broken cell");

    const Bytes png = w.CollisionPng();
    WriteFile("world_collision.png", png);
    check(png.size() > 8 && png[0] == 0x89 && png[16] == 0 && png[19] == 64 && png[23] == 64, "collision overview: 64x64 PNG (2x2 maps of 32 tiles)");

    // the quad spans 0..2 in X (scale 2): cells of 2; cell (1,1) centred at (3, 0, 3);
    // the building (the same quad) at its cell centre + (1.5, 0, -2)
    float cell = 0;
    const Narc buildings(MakeNarc({terrain}));
    const std::string g = w.Gltf(nullptr, &buildings, nullptr, &cell);
    WriteFile("world.gltf", Bytes(g.begin(), g.end()));
    check(cell == 2.0f, "world glTF: cell size measured from the terrain");
    check(g.find("\"min\":[1,0,1],\"max\":[3,2,1]") != std::string::npos && g.find("\"min\":[3,0,3],\"max\":[5,2,3]") != std::string::npos &&
          g.find("\"min\":[4.5,0,1],\"max\":[6.5,2,1]") != std::string::npos, "world glTF: terrain placed in its cells, building placed in its cell");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
