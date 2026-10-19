#ifndef REMAKE_ORASSAVE_H
#define REMAKE_ORASSAVE_H

// An Omega Ruby / Alpha Sapphire save file ("main", 0x76000 bytes), as far as the remake needs it. Checked on the owner's save:
//   a block table at 0x75E14 (after a 0x14-byte header holding the bytes "FEEB" at 0x75E10): one 8-byte entry per block, u32 length,
//   u16 block id, u16 CRC16-CCITT (initial value 0xFFFF) of the block's bytes; blocks follow each other from offset 0, each starting
//   on a 0x200 boundary. All 58 blocks of the owner's save match their checksum.
//   The layout is not a table of the game's (read 17 October, ORAS_ENGINE.md 6, Saving): each block's size is its class's size
//   getter (vtable +0x10), its offset the sum of the aligned sizes before it (Save_ReadBlocks / Save_WriteBlocks), the footer
//   (save ids, FEEB, the table) is the last 0x200 bytes, and the file's size is that sum (FUN_00115a84, at boot: no 0x76000
//   constant in the code). So oras-engine --characters N (N > 31) grows block 10 (MoveModelSave) to N + 1 records and the
//   blocks after it move: ResizeMoveModels writes a save in that layout.
//   block 4 (offset 0x1400, 336 bytes): where the player stands. u16 at +2 the zone (a/0/1/3 member; 6 in a save made in Littleroot),
//   f32 at +0x10 and +0x18 the position x and z in pixels (18 a tile, a tile's centre at +9), as the zone files hold positions
//   (inferred from the save: zone 6, x 1797.9, z 3151.0, a few tiles from Littleroot's spawn tile (100.5, 172.5)). The same two floats
//   are held again at +0x104 and +0x10C (two saves a few steps apart differ at both, with the same values); moving only the first pair
//   did not move the player on the phone.

#include "Bytes.h"
#include "OrasZone.h"

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
    // copies block <id> whole from another save (same place and size, else FormatError); MoveTo or WriteChecksums after
    void TakeBlockFrom(const OrasSave& other, uint16_t id);
    // writes every block's checksum again
    void WriteChecksums();
    // block 10 (MoveModelSave) made OrasMoveModelBytes(records) long, the records kept (the ones past the new size dropped), the
    // others zero; the blocks after it moved to their new offsets, the footer to the file's new end, the table and checksums
    // rewritten: the layout oras-engine --characters (records - 1) gives the game. 32 records is the game's own layout
    void ResizeMoveModels(size_t records);
    // block 10 given a record for each of the zone's characters, in the records MoveTo emptied (after MoveTo into `zone`): on
    // Continue the game rebuilds a zone's people from these records, not from the zone file (owner's phone, 19 October: a save
    // moved into Vaniville showed nobody until a zone change; one the game made there showed them). The layout below is seen
    // in a save the game made in Vaniville (memory after an in-game save, run hSave2: its 7 people, all standing or walking
    // in place, and the player's), not read in code. Vanish flags are not checked: a character whose flag the save has set
    // is placed all the same. Throws FormatError when the block has too few free records
    void PlaceCharacters(int zone, const std::vector<ZoneCharacter>& characters);
    // the block table's offset in the file: the footer is its last 0x200 bytes, the table at its +0x14
    size_t TableAt() const { return Data.size() - 0x200 + 0x14; }
};

// a MoveModelSave record (block 10), one a saved character
constexpr size_t OrasMoveModelRecord = 0x108;
// block 10's size for that many records (32 to 256): the records' bytes rounded up to an ARM immediate, as the patched size
// getter returns it with one `mov` (OrasEngine.cpp); 0x2100, the game's, for 32
size_t OrasMoveModelBytes(size_t records);

uint16_t Crc16Ccitt(const uint8_t* data, size_t size);

}

#endif // REMAKE_ORASSAVE_H
