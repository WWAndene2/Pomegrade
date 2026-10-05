// The remake tooling's town classification (TownLayout::Classify) on window scans built here: what each
// tile shows decides its role by a fixed precedence, fences enclose flower beds, sand-coloured half tiles
// are path, and a texture with no role is reported rather than guessed. The texture names and sand
// colours are the ones measured on Platinum's Twinleaf Town; the arithmetic built on them is checked here.
#include "TownLayout.h"

#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static TerrainScan Blank(int r)
{
    TerrainScan s;
    s.Tiles = TownTiles; s.R = r;
    s.Samples.resize((size_t)TownTiles * r * TownTiles * r);
    return s;
}

static TerrainSample& At(TerrainScan& s, int c, int r) { return s.Samples[(size_t)r * s.Tiles * s.R + c]; }

int main()
{
    check(BaseMaterialName("nsand_lm12") == "nsand", "a lightmap suffix is dropped");
    check(BaseMaterialName("tree04_2") == "tree04_2", "a name ending in a digit but no _lm keeps it");
    check(BaseMaterialName("s_snow_lm") == "s_snow_lm", "a bare _lm is not a lightmap number");

    TerrainScan whole = Blank(1), half = Blank(2);
    // grass under a tree and its shadow: the tree decides; a house over grass: the house
    At(whole, 3, 3).Materials = {"ngrass", "tree01", "tshadow"};
    At(whole, 4, 3).Materials = {"ngrass", "h_kage"};
    // water under its frame: water; a path's outline alone: grass
    At(whole, 5, 3).Materials = {"puddle_b", "puddle", "lakep"};
    At(whole, 6, 3).Materials = {"nsandp"};
    // a texture nobody gave a role
    At(whole, 7, 3).Materials = {"mystery"};
    At(whole, 8, 3).Materials = {"ngrass", "mystery"};
    // a ring of fence around one tile
    for (int c = 20; c <= 22; c++) for (int r = 20; r <= 22; r++) At(whole, c, r).Materials = {"imped"};
    At(whole, 21, 21).Materials = {"ngrass"};
    // half tiles: sand-coloured is path, grass-coloured is not, no colour is not
    At(half, 10, 10).HasColour = true; At(half, 10, 10).Rgb[0] = 230; At(half, 10, 10).Rgb[1] = 200; At(half, 10, 10).Rgb[2] = 120;
    At(half, 11, 10).HasColour = true; At(half, 11, 10).Rgb[0] = 30; At(half, 11, 10).Rgb[1] = 170; At(half, 11, 10).Rgb[2] = 80;

    TownLayout layout;
    layout.Classify(whole, half);
    check(layout.Vis.size() == (size_t)TownTiles && layout.Vis[0].size() == (size_t)TownTiles, "the layout is a full window");
    check(layout.Vis[3][3] == 't', "a tree wins over the grass and its shadow");
    check(layout.Vis[3][4] == 'H', "a house wins over grass");
    check(layout.Vis[3][5] == '~', "water wins over its frame");
    check(layout.Vis[3][6] == '.', "a path's outline alone is grass");
    check(layout.Vis[3][7] == '.' && layout.UnknownTextures.count("mystery") && layout.UnknownTextures["mystery"] == 2, "an unknown texture is counted a tile each and left as grass");
    check(layout.Vis[21][21] == '*' && layout.BedTiles == 1, "the tile a fence encloses holds flowers");
    check(layout.Vis[20][20] == 'F', "the fence stays a fence");
    check(layout.Path2[10][10] == ':' && layout.Path2[10][11] == '.' && layout.Path2[0][0] == '.', "sand-coloured half tiles are path");
    check(layout.Water2[6][10] == '~' && layout.Water2[7][11] == '~' && layout.Water2[6][12] == '.', "a water tile is four water half tiles");

    char role = 'x';
    check(TextureRole("tshadow", role) && role == 0, "a shadow has no role");
    check(!TextureRole("never_seen", role), "a name not in the table is unknown");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
