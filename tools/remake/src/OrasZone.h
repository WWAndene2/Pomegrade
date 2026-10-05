#ifndef REMAKE_ORASZONE_H
#define REMAKE_ORASZONE_H

// One zone of Omega Ruby / Alpha Sapphire (a member of a/0/1/3, a "ZO" container), as far as it is understood. Layout
// measured on the real file (538 zones) and checked on Littleroot's (zone 6): its warps lie in Littleroot's matrix cell.
//
//   file 0   56 bytes, 28 u16 words (the header): word 1 the area pack (a/0/1/4 member), word 2 the map matrix,
//            word 13 the zone's own number, words 22-24 (and again 25-27) a position in pixels (x, y, z; a pixel is
//            1/18 of a tile): Littleroot's is tile (100.5, 172.5), where the player appears. The other words are kept raw.
//   file 1   u32 size (of everything after this field, up to the end of the arrays), four u8 counts (furniture,
//            characters, warps, triggers), u32 count of a fifth kind of entry, then the arrays from offset 12 in that order:
//              furniture 0x14 bytes, characters 0x30, warps 0x18, triggers 0x18, fifth kind 0x18 (kept raw);
//            then, with no gap, a Pawn (AMX) script starting with its own length: the zone's initialisation script
//            (the file is padded to a multiple of 4). Checked on all 536 zones that are ZO containers:
//            size == 8 + 0x14 nf + 0x30 nn + 0x18 (nw + nt + n5) for every one.
//   file 2   a Pawn (AMX) script: the zone's own (people, signs, events)
//   file 4   12 bytes, zero in Littleroot's
//   file 3   empty in Littleroot's

#include "Bytes.h"

#include <array>
#include <vector>

namespace remake
{

// Words are u16 from the start of each entry. Known (read on Littleroot's, whose positions fall in its matrix cell):
// furniture: tile x, z at words 4, 5; characters: model at 1, tile x, z at 20, 21; warps: destination zone at 0, position
// in pixels (a pixel is 1/18 of a tile, a tile's centre is at +9) x at 4, z at 6; triggers: tile x, z at 6, 7.
struct ZoneFurniture  { std::array<uint16_t, 10> Raw{}; int TileX() const { return Raw[4]; } int TileZ() const { return Raw[5]; } };
struct ZoneCharacter  { std::array<uint16_t, 24> Raw{}; int Model() const { return Raw[1]; } int TileX() const { return Raw[20]; } int TileZ() const { return Raw[21]; } };
struct ZoneDoor       { std::array<uint16_t, 12> Raw{}; int DestZone() const { return Raw[0]; } float TileX() const { return Raw[4] / 18.0f; } float TileZ() const { return Raw[6] / 18.0f; } };
struct ZoneTrigger    { std::array<uint16_t, 12> Raw{}; int TileX() const { return Raw[6]; } int TileZ() const { return Raw[7]; } };
struct ZoneOther      { std::array<uint16_t, 12> Raw{}; };

struct OrasZone
{
    std::array<uint16_t, 28> Header{};
    std::vector<ZoneFurniture> Furniture;
    std::vector<ZoneCharacter> Characters;
    std::vector<ZoneDoor> Doors;
    std::vector<ZoneTrigger> Triggers;
    std::vector<ZoneOther> Others;   // the fifth kind, raw
    Bytes InitScript, Script, Trailer;

    int AreaPack() const { return Header[1]; }
    int Matrix() const { return Header[2]; }
    int Number() const { return Header[13]; }
    float SpawnTileX() const { return Header[22] / 18.0f; }
    float SpawnTileZ() const { return Header[24] / 18.0f; }

    // throws FormatError when the container, the header or the arrays' total do not fit the layout above
    static OrasZone Read(const Bytes& zo);
};

}

#endif // REMAKE_ORASZONE_H
