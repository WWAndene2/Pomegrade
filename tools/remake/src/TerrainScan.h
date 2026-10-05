#ifndef REMAKE_TERRAINSCAN_H
#define REMAKE_TERRAINSCAN_H

// A top-down look at a Platinum terrain window: at the centre of every 1/R tile,
// the names of the materials of all terrain triangles above it, and the colour
// of the topmost opaque texel (the texture's repeat applied). One DS tile is one
// ORAS tile, so a window is the same in both games' tile units.

#include "PlatinumWorld.h"

#include <set>
#include <string>
#include <vector>

namespace remake
{

struct TerrainSample
{
    std::set<std::string> Materials; // without the _lmNN lightmap suffix
    bool HasColour = false;
    uint8_t Rgb[3] = {};
    float Height = 0;                // of the colour's surface
};

struct TerrainScan
{
    int Left = 0, Top = 0;  // the window's first tile in the matrix's tile grid
    int Tiles = 0, R = 1;   // Tiles x Tiles tiles, R samples across each
    std::vector<TerrainSample> Samples; // (Tiles * R) squared, row-major

    const TerrainSample& At(int column, int row) const { return Samples[(size_t)row * Tiles * R + column]; }
    static TerrainScan Run(const PlatinumWorld& world, int left, int top, int tiles, int r);
};

// a material name without its _lmNN suffix (the lightmap variant of the same texture)
std::string BaseMaterialName(const std::string& name);

}

#endif // REMAKE_TERRAINSCAN_H
