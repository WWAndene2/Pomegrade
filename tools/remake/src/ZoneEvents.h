#ifndef REMAKE_ZONEEVENTS_H
#define REMAKE_ZONEEVENTS_H

// A Pokemon Platinum zone's events (a member of
// fielddata/eventdata/zone_event.narc, named by its map header): four lists,
// each a u32 count then fixed-size entries (layout from the pokeplatinum
// decompilation's event packer):
// - background events (0x14: signs, hidden items): script, type, x, z, y;
// - objects (0x20: people, items on the ground): local id, graphics,
//   movement, trainer type, hidden flag, script, direction, 3 data words,
//   movement range x/z, x, z (tiles), y (16.16 tiles);
// - warps (0xC: doors, stairs, cave entrances): x, z (tiles), destination
//   map header, destination warp;
// - coordinate triggers (0x10): script, x, z, width, length, y, value, var.
// Coordinates are tiles in the zone's map matrix: checked on Platinum, the
// 293 warps of Sinnoh's matrix all lie in a cell of their own zone, and all
// 534 event files read to exactly their size.

#include "Bytes.h"

#include <vector>

namespace remake
{

struct ZoneWarp
{
    uint16_t X = 0, Z = 0;
    uint16_t DestHeader = 0, DestWarp = 0;
};

struct ZoneObject
{
    uint16_t Id = 0, Graphics = 0, Script = 0, X = 0, Z = 0;
    float Y = 0; // tiles
};

struct ZoneEvents
{
    size_t BgEvents = 0, CoordEvents = 0; // counted, not decoded
    std::vector<ZoneObject> Objects;
    std::vector<ZoneWarp> Warps;

    static ZoneEvents Read(const Bytes& data);
};

}

#endif // REMAKE_ZONEEVENTS_H
