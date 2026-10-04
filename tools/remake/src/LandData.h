#ifndef REMAKE_LANDDATA_H
#define REMAKE_LANDDATA_H

// One map piece of Pokemon Diamond/Pearl/Platinum (a member of
// fielddata/land_data/land_data.narc): four u32 section sizes, then
// - movement permissions: 32x32 tiles, u16 each (low byte: tile behaviour,
//   e.g. grass, water, ledge; high byte: collision, 0x80 solid),
// - buildings: 0x30-byte entries (u32 model id in the area's building
//   archive, then x, y, z as 16.16 fixed point, then rotation and size
//   words kept raw),
// - the terrain model (NSBMD),
// - the terrain heights (BDHC, kept raw: the walkable surface's plates).
// Layout from the community documentation (Pokemon DS Map Studio); the
// buildings' coordinate frame in particular is to be checked on a real file.

#include "Bytes.h"

#include <array>
#include <vector>

namespace remake
{

constexpr uint32_t LandTiles = 32;

struct LandBuilding
{
    uint32_t Model = 0;
    float Position[3] = {};
    std::array<uint32_t, 8> Raw{}; // the entry's remaining words
};

struct LandData
{
    std::vector<uint16_t> Permissions; // LandTiles * LandTiles, row-major
    std::vector<LandBuilding> Buildings;
    Bytes Model, Heights;

    static LandData Read(const Bytes& data);
    static bool Solid(uint16_t permission) { return (permission >> 8) & 0x80; }
};

}

#endif // REMAKE_LANDDATA_H
