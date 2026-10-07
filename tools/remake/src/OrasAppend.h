#ifndef REMAKE_ORASAPPEND_H
#define REMAKE_ORASAPPEND_H

// Mods that test whether ORAS accepts archive members past the ones it ships (a 858th map piece in a/0/3/9, a 432nd matrix in
// a/0/4/0, a 539th zone in a/0/1/3): Sinnoh needs 372 overworld pieces where Hoenn has 165 (ORAS_LITTLEROOT.md 2b). Each
// added member is a copy of Littleroot's (piece 6, matrix 1, zone 6), so a run that works shows Littleroot as the game has it.
//   AppendUnused   the three members appended, nothing pointing at them: does the game still start and load the field?
//   AppendPiece    matrix 1's cell (2, 4) points at the added piece
//   AppendMatrix   zone 6's header (word 2) points at the added matrix
//   AppendZone     the added zone (own number, header word 13, its index) over Littleroot's blocks of matrix 1's zone grid
//   AppendZoneRaised  AppendZone with what the engine needs to accept it (ORAS_ENGINE.md 2): the zone header table (member
//                  536) and the encounter container (member 537) grown to the new zone count, and code.ips raising the
//                  header loaders' zone bound and the table's size check

#include "N3dsRom.h"

#include <string>
#include <vector>

namespace remake
{

enum class AppendTest { Unused, Piece, Matrix, Zone, ZoneRaised };

// writes the mod (BPS patches under <outDir>/load/mods/<program id>/romfs_ext, and exefs/code.ips for ZoneRaised) and returns
// what it did, line by line
std::vector<std::string> BuildAppendTest(N3dsRom& oras, AppendTest test, const std::string& outDir);

}

#endif // REMAKE_ORASAPPEND_H
