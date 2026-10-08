#ifndef REMAKE_ORASSAVE_H
#define REMAKE_ORASSAVE_H

// An Omega Ruby / Alpha Sapphire save file ("main", 0x76000 bytes), as far as the remake needs it. Checked on the owner's save:
//   a block table at 0x75E14 (after a 0x14-byte header holding the bytes "FEEB" at 0x75E10): one 8-byte entry per block, u32 length,
//   u16 block id, u16 CRC16-CCITT (initial value 0xFFFF) of the block's bytes; blocks follow each other from offset 0, each starting
//   on a 0x200 boundary. All 58 blocks of the owner's save match their checksum.
//   block 4 (offset 0x1400, 336 bytes): where the player stands. u16 at +2 the zone (a/0/1/3 member; 6 in a save made in Littleroot),
//   f32 at +0x10 and +0x18 the position x and z in pixels (18 a tile, a tile's centre at +9), as the zone files hold positions
//   (inferred from the save: zone 6, x 1797.9, z 3151.0, a few tiles from Littleroot's spawn tile (100.5, 172.5)). The same two floats
//   are held again at +0x104 and +0x10C (two saves a few steps apart differ at both, with the same values); moving only the first pair
//   did not move the player on the phone.

#include "Bytes.h"

#include <vector>

namespace remake
{

struct OrasSaveBlock { size_t Offset = 0, Length = 0; uint16_t Id = 0, Checksum = 0; };

struct OrasSave
{
    Bytes Data;
    std::vector<OrasSaveBlock> Blocks;

    // throws FormatError when the table is not found or a block does not match its checksum
    static OrasSave Read(const Bytes& data);
    int Zone() const;
    float X() const;
    float Z() const;
    // moves the player (zone, and the tile's centre in pixels) and writes block 4's checksum again
    void MoveTo(int zone, float tileX, float tileZ);
    // copies the whole player block (block 4) from another save (one the game made in the target matrix), before MoveTo: the
    // block's other words (last warp, a second zone and position record, ...) then come from a place the game wrote itself
    void TakePlayerBlockFrom(const OrasSave& other);
};

uint16_t Crc16Ccitt(const uint8_t* data, size_t size);

}

#endif // REMAKE_ORASSAVE_H
