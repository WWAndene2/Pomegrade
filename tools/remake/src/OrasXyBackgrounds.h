#ifndef REMAKE_ORASXYBACKGROUNDS_H
#define REMAKE_ORASXYBACKGROUNDS_H

// X/Y's own battle backgrounds for its zones (owner, 22 October: a background follows the place and the context, not the
// game). Both games pick one the same way (read 22 October, ORAS_ENGINE.md 0.1): a zone's background value (header
// halfword 15, bits 7-13) gives a 10-byte record (FUN_0048AA44, X's 0x45A5C8) from a table of 2 bytes a value (record
// bytes 0-1), then at +2N 5 bytes a value (bytes 2, 3, 4, 8, 9); byte 5 is drawn from the 12-byte entry byte 4 names
// (pairs scenery, weight out of 100 from +4) and bytes 6-7 from the 0x18-byte entry byte 5 names (a species, from +6).
// DllBackGround (FUN_100043E4) loads a/1/3/6 members by the record: byte 3 the backdrop (a member, and two 0x30-byte
// light tables of 4 times of day), byte 0 the ground, byte 4 a scenery set's member (its 12-byte entry, the same as the
// static one), byte 5 a scenery's (the 0x18-byte entry, the same as the static one); 0 loads none. DllBattle loads the
// platform by byte 2 (its table at 0x10106074; 0 none). Bytes 1, 8 and 9 are small numbers both games use alike (b1 0-1,
// b8 0-12 in Omega Ruby and 0-7 in X, b9 0-2), carried as they are (inferred: same meaning).
//
// A battle's record is the zone's value's, its bytes 2-9 another value's when the battle names one (FUN_003EAB6C, read 22
// October): a trainer battle's script gives two (_CallTrainerBattleCore's arguments 4 and 5, the record's two parts: Grant's
// X/Y 49 and 49, his arena's backdrop btl_G_gym1_copy2_c03, which Omega Ruby's 49 made a cave, run bgk1), or the tile under
// the player its bits 17-23 (FUN_00403190, XyPieceToOras). The values are numbered as they are met (XyBackgroundValue), the
// tables built for them once all are (AddXyBackgrounds).
//
// Omega Ruby's 93 values leave 35 of the 128 a zone's header and a tile can name (7 bits), fewer than X/Y's zones and
// tiles and scripts name together: a script's value is a whole number, FUN_0048AA44 takes any, so the table is made 256
// values long and the values only scripts name are numbered from 128 (a value named both ways has two numbers, the same
// record).
//
// Each X/Y value given takes its number with X's record, every
// index in it pointing at a new entry of Omega Ruby's table (X's entry, its members converted: BCH 0x07 -> 0x21, inside
// the "BG" containers too) or at Omega Ruby's own entry when it is the same; a member identical to one of Omega Ruby's is
// shared. Omega Ruby has one scenery set and one scenery (both never drawn: every record's byte 4 is 0), X has its trees,
// rocks, snow and sand (btl_V_*): they come with X's records. The tables, longer, are oras-engine's to place
// (--xy-backgrounds, OrasEngine.cpp): Tables() is the file it reads.

#include "Bytes.h"
#include "N3dsRom.h"
#include "OrasWorkspace.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

struct XyBackgrounds
{
    // Omega Ruby's tables, each the game's entries then the added ones: the records (256 values, 2 bytes each, then 5 bytes
    // each), the scenery sets (12 bytes), the sceneries (0x18), the backdrops' members (4), their two light tables (0x30
    // each), the grounds' members (4) and the platforms' (4)
    Bytes Records, Sets, Sceneries, Backdrops, LightA, LightB, Grounds, Platforms;
    // the file oras-engine --xy-backgrounds reads: "XYBG", then each table above as a u32 size and its bytes
    Bytes Tables() const;
};

// X/Y's background values given Omega Ruby's numbers: 93-127 (Narrow, for a header or a tile) and 128-255 (Wide, for a script)
struct XyBackgroundNumbers
{
    std::map<int, int> Narrow, Wide; // X/Y's value -> Omega Ruby's
    bool Empty() const { return Narrow.empty() && Wide.empty(); }
};

// X/Y's background value x as Omega Ruby's: its number, the next free one the first time it is met (narrow: below 128, for a
// header or a tile; otherwise its narrow number when it has one); 0 (none) stays 0
int XyBackgroundValue(XyBackgroundNumbers& numbers, int x, bool narrow);

// X/Y's values given Omega Ruby's numbers made Omega Ruby's: their members appended to a/1/3/6 (ws), the log's lines added
XyBackgrounds AddXyBackgrounds(OrasWorkspace& ws, N3dsRom& xRom, const XyBackgroundNumbers& numbers, std::vector<std::string>& log);

}

#endif // REMAKE_ORASXYBACKGROUNDS_H
