// The remake tooling's town classification (TownLayout::Classify) on window scans built here: what each
// tile shows decides its role by a fixed precedence, fences enclose flower beds, sand-coloured half tiles
// are path, and a texture with no role is reported rather than guessed. The texture names and sand
// colours are the ones measured on Platinum's Twinleaf Town; the arithmetic built on them is checked here.
#include "NitroCompression.h"
#include "OrasTown.h"
#include "TownCheck.h"
#include "TownLayout.h"

#include <cstdio>
#include <cstring>
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
    // a yellow flower in that bed (half tile 43, 43 is in tile 21, 21, away from the crossroads below) is sand-coloured
    // half tiles: sand-coloured is path, grass-coloured is not, no colour is not
    At(half, 10, 10).HasColour = true; At(half, 10, 10).Rgb[0] = 230; At(half, 10, 10).Rgb[1] = 200; At(half, 10, 10).Rgb[2] = 120;
    At(half, 11, 10).HasColour = true; At(half, 11, 10).Rgb[0] = 30; At(half, 11, 10).Rgb[1] = 170; At(half, 11, 10).Rgb[2] = 80;

    // a crossroads, half tiles 40-60: arms 4 wide, Platinum's rounding in its inner corner above and to the left
    // of the centre, one half tile at (37, 39); the corners of the arms' ends stay
    auto sand = [&](int c, int r) { TerrainSample& s = At(half, c, r); s.HasColour = true; s.Rgb[0] = 230; s.Rgb[1] = 200; s.Rgb[2] = 120; };
    for (int k = 30; k <= 50; k++) for (int w = 38; w <= 41; w++) { sand(w, k); sand(k, w); }
    sand(37, 37);
    sand(43, 43);

    // snow (white half tiles): a 3 x 3 blob with a pinhole at its centre (60, 60 to 62, 62), a lone white speck (70, 70)
    auto white = [&](int c, int r) { TerrainSample& t = At(half, c, r); t.HasColour = true; t.Rgb[0] = 240; t.Rgb[1] = 245; t.Rgb[2] = 250; };
    for (int c = 60; c <= 62; c++) for (int r = 60; r <= 62; r++) if (c != 61 || r != 61) white(c, r);
    white(70, 70);
    // a dark grey half tile (a tree's shadow over the snow) at (63, 61), with snow on two sides: (62, 61) and the white (63, 62)
    auto grey = [&](int c, int r) { TerrainSample& t = At(half, c, r); t.HasColour = true; t.Rgb[0] = 70; t.Rgb[1] = 80; t.Rgb[2] = 72; };
    white(63, 62); grey(63, 61);
    // a tree on tile (32, 30) (half tiles 64-65, 60-61), beside the blob: the snow goes on under it, a tile, and no further
    At(whole, 32, 30).Materials = {"tree01"};
    At(whole, 33, 30).Materials = {"tree01"};
    // a green half tile beside the snow stays grass: (59, 61)
    { TerrainSample& t = At(half, 59, 61); t.HasColour = true; t.Rgb[0] = 90; t.Rgb[1] = 240; t.Rgb[2] = 150; }
    // and white on the fence tile (20, 20): half tile (40, 40)
    white(40, 40); white(41, 40);

    TownLayout layout;
    layout.Classify(whole, half);
    check(layout.Snow2.size() == 80 && layout.Snow2[60][60] == '#' && layout.Snow2[62][62] == '#', "white half tiles are snow");
    check(layout.Snow2[61][61] == '#', "a pinhole in the snow is filled");
    check(layout.Snow2[70][70] == '.', "a lone white speck is dropped");
    check(layout.Snow2[61][63] == '#', "a grey shadow with snow on two sides is snow");
    check(layout.Snow2[61][59] == '.', "green grass beside the snow is not");
    check(layout.Snow2[61][64] == '#' && layout.Snow2[61][65] == '#', "the snow goes on under a tree beside it");
    check(layout.Snow2[61][66] == '.', "a tile under the trees, no further");
    check(layout.Snow2[40][40] == '.' && layout.Snow2[40][41] == '.', "no snow on a fence tile");
    check(layout.Path2[37][37] == '.', "a crossroads' rounded inner corner is not a step of path");
    check(layout.Path2[43][43] == '.', "a sand-coloured flower in a bed is not path");
    check(layout.Path2[38][37] == ':' && layout.Path2[37][38] == ':' && layout.Path2[38][38] == ':', "the arms beside it stay path");
    check(layout.Path2[30][38] == ':' && layout.Path2[30][41] == ':' && layout.Path2[38][30] == ':' && layout.Path2[41][50] == ':', "an arm's outer corners stay path");
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

    // an archive member put back as the game keeps it: compressed once, read back as the container it is
    {
        Garc g(Bytes{'C', 'R', 'A', 'G', 0x1C, 0, 0, 0, 0xFF, 0xFE, 0, 4, 4, 0, 0, 0, 0x28, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0,
                     'O', 'T', 'A', 'F', 12, 0, 0, 0, 0, 0, 0xFF, 0xFF, 'B', 'T', 'A', 'F', 12, 0, 0, 0, 0, 0, 0, 0,
                     'B', 'M', 'I', 'F', 12, 0, 0, 0, 0, 0, 0, 0});
        Bytes zone = {'Z', 'O', 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        for (int i = 0; i < 8; i++) zone.insert(zone.end(), zone.begin() + 4, zone.begin() + 20); // something the compressor can shrink
        g.Set(0, Lz11Compress(zone));
        g.Set(1, Bytes{'Z', 'O', 1});
        const Garc original(g.Write());
        Bytes changed = zone;
        changed[10] = 99;
        Garc out(original.Write());
        ReplaceMember(out, original, 0, changed, "ZO");
        const Garc back(out.Write());
        check(IsLzCompressed(back.Sub(0)) && LzDecompress(back.Sub(0)) == changed, "a compressed member stays compressed once and reads back as the container");
        ReplaceMember(out, original, 1, Bytes{'Z', 'O', 7}, "ZO");
        check(Garc(out.Write()).Sub(1) == Bytes({'Z', 'O', 7}), "an uncompressed member stays uncompressed");
        bool refused = false;
        try { ReplaceMember(out, original, 0, Lz11Compress(changed), "ZO"); } catch (const FormatError&) { refused = true; }
        check(refused, "a member compressed before the call (it would be compressed twice) is refused");
    }

    // design rules: a texture the area pack lacks is an error, the budget is a warning
    {
        BchModel model;
        model.Materials.resize(3);
        model.Materials[0].Name = "grass"; model.Materials[0].Texture[0] = "chip_kusa";
        model.Materials[1].Name = "edge"; model.Materials[1].Texture[0] = "projection_dummy"; model.Materials[1].Texture[1] = "chip_grass_edge";
        model.Materials[2].Name = "unused"; model.Materials[2].Texture[0] = "nowhere";
        model.Meshes.resize(3);
        for (size_t i = 0; i < 3; i++) { model.Meshes[i].Material = i; model.Meshes[i].Vertices.resize(10); model.Meshes[i].Triangles = {0, 1, 2}; }
        model.Meshes[2].Triangles.clear(); // draws nothing: its texture is not needed
        auto issues = CheckMaterials(model, {"chip_kusa", "chip_grass_edge"});
        check(issues.empty(), "textures present in the pack (the projection placeholder and an unused mesh's need none): no issue");
        issues = CheckMaterials(model, {"chip_kusa"});
        check(issues.size() == 1 && issues[0].Error && issues[0].Text.find("chip_grass_edge") != std::string::npos, "a texture missing from the area pack is an error that names it");
        model.Meshes[2].Triangles = {0, 1, 2};
        check(CheckMaterials(model, {"chip_kusa", "chip_grass_edge"}).size() == 1, "a drawn mesh's missing texture is reported once");
        PieceBudget budget; budget.MaxFileBytes = 1000; budget.MaxVertices = 20;
        issues = CheckBudget(model, 500, budget);
        check(issues.size() == 1 && !issues[0].Error, "more vertices than the game's largest piece is a warning, not an error");
        check(CheckBudget(model, 500, PieceBudget{1000, 100}).empty(), "within the game's largest piece: no issue");
        check(CheckBudget(model, PieceBytesShown, PieceBudget{2000000, 100}).empty(), "the largest piece seen to show on the phone: no issue");
        issues = CheckBudget(model, PieceBytesShown + 1, PieceBudget{2000000, 100});
        check(issues.size() == 1 && issues[0].Error, "a piece larger than any seen to show on the phone is an error, even within the game's largest");
        model.Meshes[0].Vertices.resize(70000);
        bool tooMany = false;
        for (const TownIssue& i : CheckBudget(model, 500, PieceBudget{1000, 1000000})) tooMany = tooMany || i.Error;
        check(tooMany, "a mesh past 16-bit indices is an error");
    }

    // design rules: the layout blocks follow what the game's own 857 pieces do
    {
        auto put32 = [](Bytes& b, size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
        auto putf = [&](Bytes& b, size_t at, float f) { uint32_t v; memcpy(&v, &f, 4); put32(b, at, v); };
        Bytes tiles(4 + 1600 * 4, 0);
        tiles[0] = 40; tiles[2] = 40;
        Bytes doors(4 + 44, 0);
        put32(doors, 0, 1);
        putf(doors, 4 + 4, 1); putf(doors, 4 + 8, 1); putf(doors, 4 + 12, 1);
        putf(doors, 4 + 28, 27); putf(doors, 4 + 36, 45);
        auto has = [](const std::vector<TownIssue>& v, bool error, const char* word) {
            for (const TownIssue& i : v) if (i.Error == error && i.Text.find(word) != std::string::npos) return true;
            return false; };
        const bool zeroOk = TileValueEstablished(0);
        auto issues = CheckLayout(tiles, doors);
        check(zeroOk ? issues.empty() : has(issues, false, "tile value"), "a 40 x 40 block of an established value, a unit door on a tile centre: no issue (or only the value warning)");
        put32(tiles, 4, 0xDEADBEEF);
        check(has(CheckLayout(tiles, doors), false, "0xDEADBEEF"), "an unestablished tile value is a warning that names it");
        tiles[0] = 39;
        check(has(CheckLayout(tiles, doors), true, "40 x 40"), "a tile block that is not 40 x 40 is an error");
        putf(doors, 4 + 4, 0.8f);
        check(has(CheckLayout(Bytes(), doors), false, "scaled"), "a scaled door model is a warning");
        putf(doors, 4 + 4, 1); putf(doors, 4 + 20, 45); putf(doors, 4 + 28, 20);
        issues = CheckLayout(Bytes(), doors);
        check(has(issues, false, "multiple of 90") && has(issues, false, "centre"), "a door turned 45 degrees and off a tile centre is warned twice");
        put32(doors, 0, 9);
        check(has(CheckLayout(Bytes(), doors), true, "9 doors"), "a door count past the block is an error");
    }

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
