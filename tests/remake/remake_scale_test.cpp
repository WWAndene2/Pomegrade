// The remake tooling's DS to ORAS scale translation (N3dsWorld): a 2x2 DS
// world (64x64 tiles, one empty cell) built here, cut again into 40-tile ORAS
// pieces. The units it relies on (16 per DS tile, 18 per ORAS tile, buildings
// in tiles from their piece's centre) were measured on the real games' files;
// this checks the arithmetic built on them.
#include "N3dsWorld.h"
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

static LandData Land()
{
    LandData l;
    l.Permissions.assign(LandTiles * LandTiles, 0);
    return l;
}

int main()
{
    // matrix 2x2, cell (0,1) empty
    Bytes mx = {2, 2, 0, 0, 1, 'w'};
    for (uint16_t id : {0, 1, 0xFFFF, 2}) Push16(mx, id);
    WorldMap w(MapMatrix::Read(mx), Narc(MakeNarc({})));
    w.Errors.clear();
    w.Cells.assign(4, std::nullopt);

    LandData c00 = Land(), c10 = Land(), c11 = Land();
    c00.Permissions[0] = 0x8000;                    // tile (0,0): solid
    c00.Permissions[31 * LandTiles + 31] = 5;       // tile (31,31)
    c00.Buildings.push_back({9, {-20, 0, 0}, {}});  // 4 tiles left of the world: dropped
    c10.Permissions[3 * LandTiles + 13] = 7;        // world tile (45,3): ORAS piece (1,0), tile (5,3)
    c11.Permissions[8 * LandTiles + 0] = 3;         // world tile (32,40): ORAS piece (0,1), tile (32,0)
    c11.Buildings.push_back({4, {1.5f, 2, -2}, {}}); // world tile (49.5, 46): ORAS piece (1,1)
    w.Cells[0] = c00; w.Cells[1] = c10; w.Cells[3] = c11;

    const N3dsWorld o = N3dsWorld::Translate(w);
    check(o.Width == 2 && o.Height == 2 && o.Pieces.size() == 4, "64x64 DS tiles: 2x2 ORAS pieces of 40, all holding DS tiles");
    auto piece = [&](uint32_t x, uint32_t y) -> const N3dsPiece* {
        for (const N3dsPiece& p : o.Pieces) if (p.X == x && p.Y == y) return &p;
        return nullptr;
    };
    const N3dsPiece *p00 = piece(0, 0), *p10 = piece(1, 0), *p01 = piece(0, 1), *p11 = piece(1, 1);
    check(p00 && p10 && p01 && p11, "pieces found by position");
    if (!(p00 && p10 && p01 && p11)) { printf("FAILURES\n"); return 1; }

    check(p00->Permissions[0] == 0x8000 && p00->Permissions[31 * 40 + 31] == 5, "DS tiles keep their index in the world's grid");
    check(p10->Permissions[3 * 40 + 5] == 7, "a DS tile past the first 40 lands in the next ORAS piece");
    check(p01->Permissions[0] == -1 && p01->Permissions[32] == 3, "the empty DS cell gives no tile (-1); its neighbour's tiles are kept");
    check(p10->Permissions[39] == -1, "tiles beyond the DS world (x 64..79): none");

    check(p00->Buildings.empty(), "a building outside the world is dropped");
    check(p11->Buildings.size() == 1 && p11->Buildings[0].Model == 4 && p11->Buildings[0].SourceCell == 3, "the building moves to the ORAS piece it stands in");
    if (!p11->Buildings.empty())
    {
        const float* at = p11->Buildings[0].Position;
        // (49.5, 46) tiles from the world's corner; the piece's centre is at (60, 60): -10.5 and -14 tiles
        check(at[0] == -189.0f && at[2] == -252.0f, "building position: from its ORAS piece's centre, 18 units a tile");
        check(at[1] == 36.0f, "building height: 2 tiles, 36 ORAS units");
    }

    check(o.Scale() == 1.125f && N3dsWorld::Translate(w, 8).Scale() == 2.25f, "scale: 18 over the DS tile's size");
    const std::string json = o.Json();
    check(json.find("\"format\": \"pomegrade-remake-n3ds-world\"") != std::string::npos && json.find("\"scale\": 1.125") != std::string::npos &&
          json.find("\"position\": [-189, 36, -252], \"sourceCell\": 3") != std::string::npos, "JSON: format, scale, building");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
