#ifndef REMAKE_AREADATA_H
#define REMAKE_AREADATA_H

// A Pokemon Platinum area (a member of fielddata/areadata/area_data.narc,
// named by a map header): 4 u16, the building set (its textures in
// area_build_model/areabm_texset.narc), the map texture set
// (area_map_tex/map_tex_set.narc), a dummy, the lighting set. Layout from
// the pokeplatinum decompilation's AreaDataFile; all 75 Platinum areas are
// 8 bytes.

#include "Bytes.h"

namespace remake
{

struct AreaData
{
    uint16_t BuildingSet = 0, MapTextures = 0, Light = 0;

    static AreaData Read(const Bytes& d)
    {
        if (d.size() != 8) throw FormatError("area data: " + std::to_string(d.size()) + " bytes, not 8");
        return {U16(d, 0), U16(d, 2), U16(d, 6)};
    }
};

}

#endif // REMAKE_AREADATA_H
