#ifndef REMAKE_ORASXYENCOUNTERS_H
#define REMAKE_ORASXYENCOUNTERS_H

// An X/Y zone's wild encounters (its file 3, 392 bytes in the 53 zones that have any, none in the other 307) made Omega Ruby's
// (file 3 and its member 537 entry, 260 bytes: ORAS_ENGINE.md 6, Wild encounter selection). Both are a header of one byte
// per table, then 4-byte slots (species bits 0-10, form 11-15; minimum level; maximum level; 00 00 01 01 an empty slot).
// Seen on X (19 October), the table offsets fitting the 392 bytes exactly:
//   X/Y     header 16 bytes: 0-4 the five land tables, 5 surf, 6 rock smash, 7-9 the three rods, 10 hordes; then the land
//           tables 0-4 (12 slots each, +0x10), surf (5, +0x100), rock smash (5, +0x114), rods (3 each, +0x128), hordes
//           (3 x 5, +0x14C)
//   Omega   header 14 bytes: byte k of kind k (2: kind 2's mode); kinds 0 and 1 (12 slots, +0xE), 2 (3), surf (5), rock
//           smash (5), rods (3 each), hordes (3 x 5, +0xC6), padded to 260
// X/Y's land tables by the tile kinds (tile value bits 24-31) where they are switched on, counted over its 53 zones: table 0
// on 0x20 (grass), 0x24 (caves) and 0x25; table 1 on 0x21, 2 on 0x22, 3 on 0x23 (the flower beds, inferred from X/Y's own
// yellow, purple and red flowers); table 4 on 0x27, 0x28, 0x29, 0x32, 0x33 and 0x34 (0x31 too where table 4 is on, and
// without it in zone 270: inferred). Omega Ruby has two land tables: kind 0 on 0x1F, 0x20, 0x24, 0x25, 0x2B, 0x2D, 0x42 and
// kind 1 on 0x27 (read). So the tiles of tables 1-3 are made 0x20 and those of table 4 0x27 (XyTileValue), and a zone's
// kind 0 is the one of its tables 0-3 with the most tiles; the others are left out (logged: 11 zones have two or three).

#include "Bytes.h"
#include "Garc.h"
#include "OrasMatrix.h"

#include <array>
#include <string>
#include <vector>

namespace remake
{

// the number of tiles of X/Y's land tables 0-3 in the zone (the matrix's zone grid, or all of it without one)
std::array<size_t, 4> XyLandTiles(int xZone, const OrasMatrix& xMatrix, const Garc& xPieces);

// X/Y's file 3 made Omega Ruby's (empty for an empty one)
Bytes XyEncountersToOras(const Bytes& xFile, const std::array<size_t, 4>& landTiles, int xZone, std::vector<std::string>& log);

}

#endif // REMAKE_ORASXYENCOUNTERS_H
