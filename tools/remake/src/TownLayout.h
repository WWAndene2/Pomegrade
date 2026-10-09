#ifndef REMAKE_TOWNLAYOUT_H
#define REMAKE_TOWNLAYOUT_H

// What a Platinum window of 40x40 tiles is made of, read from the game's own
// data: each tile's role (what its terrain shows from above, decided by the
// textures), the paths and the water at half-tile precision, the collision, and
// the doors (the zone's warps). This is the input of the ORAS town builder.
//
// Role characters (Vis): T forest, t tree, : path, s pale grass, * flowers,
// ~ water, f a pond's frame, H house, F fence, L ledge, g tall grass, . grass.

#include "TerrainScan.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace remake
{

constexpr int TownTiles = 40; // an ORAS map piece

struct TownDoor { int Column = 0, Row = 0; uint16_t Zone = 0, Warp = 0, DestZone = 0, DestWarp = 0; };

// a Platinum building model placed on the window (furniture, a window on a wall, stairs: a room's objects are these): its
// model's name, its centre in tiles of the window and its height in tiles
struct TownObject { std::string Name; float Column = 0, Row = 0, Height = 0; };

struct TownLayout
{
    int Left = 0, Top = 0;               // the window's first tile in the matrix's tile grid
    std::vector<std::string> Vis;        // TownTiles rows of TownTiles roles
    std::vector<std::string> Path2;      // 2*TownTiles rows: ':' where a half tile is path
    std::vector<std::string> Water2;     // 2*TownTiles rows: '~' where a half tile is water
    std::vector<std::string> Snow2;      // 2*TownTiles rows: '#' where a half tile is snow (white), specks and pinholes cleaned
    std::vector<std::string> Collision;  // TownTiles rows: '#' solid, '~' water, 'g' tall grass, '.' free
    std::vector<TownDoor> Doors;
    std::vector<TownObject> Objects;
    std::map<std::string, int> UnknownTextures; // textures with no role: counted, left as grass
    // each tile's Platinum materials (TownTiles rows of TownTiles sets, row-major): an interior's furniture (RoomBuilder.h)
    std::vector<std::vector<std::set<std::string>>> TileMaterials;
    int BedTiles = 0;                    // tiles a fence encloses, filled with flowers

    static TownLayout Read(const PlatinumWorld& world, int left, int top);
    // Vis, Path2, Water2, Snow2, BedTiles and UnknownTextures from two scans of the window (one sample a tile, two a tile)
    void Classify(const TerrainScan& whole, const TerrainScan& half);
    std::string Text() const;            // the layout as text, for inspection
};

// the role a texture has, or '\0' when it has none (shadows, a path's outline, ...); false when unknown
bool TextureRole(const std::string& baseName, char& role);

}

#endif // REMAKE_TOWNLAYOUT_H
