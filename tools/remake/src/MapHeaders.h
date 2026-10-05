#ifndef REMAKE_MAPHEADERS_H
#define REMAKE_MAPHEADERS_H

// Pokemon Platinum's map headers: one per zone (a town, a route, a house),
// naming its area (textures, buildings), map matrix, event file and camera.
// A table of 24-byte entries in the ARM9 binary (layout from the pokeplatinum
// decompilation's MapHeader: u8 area, u8 preloaded objects, u16 matrix, u16
// scripts, u16 init scripts, u16 texts, u16 day music, u16 night music, u16
// encounters, u16 events, u16 map label, u8 weather, u8 camera type, u16 flags).
// Its address differs between versions, so it is found: among the runs of
// entries whose area, matrix and event file all exist in the cartridge's
// archives, the one naming the most different event files (zeroed memory
// passes the first test but names one). Checked on the French Platinum
// (CPUF): 593 headers at ARM9 + 0xE60A4 naming all 534 event files, every
// matrix and event file as the decompilation lists them; the next run names
// 23.

#include "Bytes.h"

#include <vector>

namespace remake
{

struct MapHeader
{
    uint8_t Area = 0;
    uint16_t Matrix = 0, Events = 0;
    uint8_t Weather = 0, Camera = 0; // camera: an index into the game's camera table
};

// areas, matrices, events: the member counts of area_data.narc, map_matrix.narc, zone_event.narc;
// at: where the table was found in arm9. Throws when no run names 100 different event files.
std::vector<MapHeader> FindMapHeaders(const Bytes& arm9, size_t areas, size_t matrices, size_t events, size_t* at = nullptr);

}

#endif // REMAKE_MAPHEADERS_H
