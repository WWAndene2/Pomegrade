#ifndef REMAKE_PLATINUMWORLD_H
#define REMAKE_PLATINUMWORLD_H

// One Platinum map matrix with everything needed around it: the matrix's cells
// (WorldMap), the textures each cell's area uses, and the warps of every zone
// on the matrix. Shared by the commands that translate a Platinum world to
// Omega Ruby / Alpha Sapphire (oras-world, oras-town).

#include "AreaData.h"
#include "MapHeaders.h"
#include "N3dsWorld.h"
#include "NdsRom.h"
#include "WorldMap.h"

#include <map>
#include <memory>
#include <vector>

namespace remake
{

class PlatinumWorld
{
public:
    // throws FormatError when the cartridge is not Platinum's or the matrix does not exist
    PlatinumWorld(const NdsRom& rom, size_t matrix);

    size_t Matrix;
    Narc Areas, Events, MapTextures, BuildingTextures, BuildingModels;
    std::vector<MapHeader> Headers;
    size_t HeaderTableAt = 0;
    WorldMap World;
    std::vector<CellTextures> CellTex;  // per matrix cell: its zone's area's texture sets
    std::vector<NdsWarp> Warps;         // every zone on this matrix, in the matrix's tiles
    size_t MapTextureSets() const { return MapSets.size(); }

private:
    // texture sets are decoded once and shared by the cells that use them
    std::map<uint16_t, Bytes> MapFiles, BuildingFiles;
    std::map<uint16_t, std::unique_ptr<Tex0>> MapSets, BuildingSets;
    const Tex0* Set(const Narc& narc, uint16_t id, std::map<uint16_t, Bytes>& files, std::map<uint16_t, std::unique_ptr<Tex0>>& sets);
};

}

#endif // REMAKE_PLATINUMWORLD_H
