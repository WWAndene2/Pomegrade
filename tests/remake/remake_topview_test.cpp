// The top view (TopView.h) on a hand-made piece whose picture is known: the kind of each material, a flat-coloured triangle,
// the drawing order (ground under a path), the tile grid, the tile values and a door.
#include "TopView.h"

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

static BchMaterial Material(const char* name, const char* texture)
{
    BchMaterial m;
    m.Name = name; m.Texture[0] = texture;
    return m;
}

// a square of tile coordinates [c0, c1] x [r0, r1] as two triangles of a mesh
static BchMesh Square(uint16_t material, float c0, float r0, float c1, float r1)
{
    BchMesh mesh;
    mesh.Material = material;
    const float corner[4][2] = {{c0, r0}, {c1, r0}, {c1, r1}, {c0, r1}};
    for (const auto& p : corner) { BchVertex v; v.Position[0] = (p[0] - 20) * 18; v.Position[2] = (p[1] - 20) * 18; mesh.Vertices.push_back(v); }
    mesh.Triangles = {0, 1, 2, 0, 2, 3};
    return mesh;
}

static void Pixel(const Bytes& rgba, int w, int x, int y, uint8_t out[3]) { for (int k = 0; k < 3; k++) out[k] = rgba[((size_t)y * w + x) * 4 + k]; }

int main()
{
    check(ClassifyMesh(Material("chip_kusa_", "chip_kusa_a")) == TopViewKind::Ground, "chip_kusa is ground");
    check(ClassifyMesh(Material("chip_kusa_b", "chip_kusa_b")) == TopViewKind::Light, "chip_kusa_b is the lighter grass");
    check(ClassifyMesh(Material("chip_soil", "chip_soil_a")) == TopViewKind::Path, "chip_soil is a path");
    check(ClassifyMesh(Material("chip_grass_decolate", "chip_alpha")) == TopViewKind::Blades, "chip_grass_decolate is the blades");
    check(ClassifyMesh(Material("chip_edge_tex", "projection_dummy")) == TopViewKind::Rim, "chip_edge_tex is the rim");
    check(ClassifyMesh(Material("shadow1", "chip_wood_shadow")) == TopViewKind::Shadow, "a shadow is not taken for wood");
    check(ClassifyMesh(Material("water1", "chip_river_a")) == TopViewKind::Water, "water1 is water");
    check(ClassifyMesh(Material("touka_house2", "touka_house01")) == TopViewKind::Structure, "a house is a structure");
    check(ClassifyMesh(Material("chip_wood_b", "chip_wood_b")) == TopViewKind::Forest, "chip_wood_b is the forest floor, not a structure");
    check(ClassifyMesh(Material("gake_basic", "gake_01_01")) == TopViewKind::Cliff, "gake is a cliff, under the ground");
    check(ClassifyMesh(Material("Comb12", "chip_wind")) == TopViewKind::Effect, "chip_wind is an effect");
    check(ClassifyMesh(Material("unknown", "nothing")) == TopViewKind::Other, "anything else is other");

    BchModel model;
    model.Materials = {Material("chip_kusa_", "chip_kusa_a"), Material("chip_soil", "chip_soil_a")};
    model.Meshes = {Square(1, 10, 10, 12, 12), Square(0, 5, 5, 15, 15)}; // the path is listed first: the ground must still be under it
    TopViewOptions options;
    options.PixelsPerTile = 8;
    Bytes tiles(4 + 1600 * 4, 0);
    Bytes doors(4 + 44, 0);
    const uint32_t one = 1;
    std::memcpy(&doors[0], &one, 4);
    const float doorX = (40 + 11) * 18 + 9.0f, doorZ = (40 + 3) * 18 + 9.0f; // world coordinates: cell 1, tile (11, 3) within the piece
    std::memcpy(&doors[4 + 28], &doorX, 4); std::memcpy(&doors[4 + 36], &doorZ, 4);

    const int w = 40 * options.PixelsPerTile;
    auto at = [&](const Bytes& rgba, double col, double row, uint8_t out[3]) { Pixel(rgba, w, (int)(col * options.PixelsPerTile), (int)(row * options.PixelsPerTile), out); };
    auto same = [](const uint8_t a[3], const uint8_t b[3]) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; };
    uint8_t ground[3], path[3], got[3];
    TopViewColour(TopViewKind::Ground, ground); TopViewColour(TopViewKind::Path, path);

    const Bytes plain = RenderTopViewRgba(model, tiles, doors, options);
    check(plain.size() == (size_t)w * w * 4, "the picture is 40 tiles square");
    at(plain, 7.5, 7.5, got);
    check(same(got, ground), "ground where only the ground is");
    at(plain, 11, 11, got);
    check(same(got, path), "the path is over the ground though it is listed first");
    at(plain, 2.5, 2.5, got);
    check(got[0] == 24 && got[1] == 24 && got[2] == 24, "nothing drawn: the background");

    TopViewOptions withGrid = options; withGrid.Grid = true;
    const Bytes grid = RenderTopViewRgba(model, tiles, doors, withGrid);
    at(grid, 7.0, 7.5, got); // on a lattice line (column 7, every fifth is stronger)
    uint8_t line[3]; at(grid, 7.5, 7.5, line);
    check(got[0] > line[0], "a tile line is lighter than the tile beside it");
    check(RenderTopViewRgba(model, tiles, doors, options) == plain, "the same piece and options give the same picture");

    TopViewOptions withTiles = options; withTiles.Tiles = true;
    const Bytes coloured = RenderTopViewRgba(model, tiles, doors, withTiles);
    at(coloured, 7.5, 7.5, got);
    check(!same(got, ground), "a tile value tints its tile");

    TopViewOptions withDoors = options; withDoors.Doors = true;
    const Bytes marked = RenderTopViewRgba(model, tiles, doors, withDoors);
    at(marked, 11.5, 3.5, got);
    check(got[0] == 255 && got[1] == 40 && got[2] == 40, "a door is drawn red on its tile within the piece (world coordinates modulo 40)");
    at(plain, 11.5, 3.5, got);
    check(!(got[0] == 255 && got[1] == 40), "and not without the option");

    const Bytes png = RenderTopView(model, tiles, doors, options);
    check(png.size() > 64 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G', "the PNG wraps the same picture");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
