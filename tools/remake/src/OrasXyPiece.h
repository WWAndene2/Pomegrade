#ifndef REMAKE_ORASXYPIECE_H
#define REMAKE_ORASXYPIECE_H

// A Pokemon X/Y map piece (an a/0/4/1 member, a GR container like Omega Ruby's a/0/3/9: ORAS_ENGINE.md 7) made Omega Ruby's,
// for a cell of a new matrix (oras-sandbox's `xy-piece`). X's piece has six files, Omega Ruby's seven (and with one or two
// more layers, X's 8 or 10 and Omega Ruby's 9 or 11, their grids and collisions after the files below, grids mapped as file 0):
//   file 0  the 40 x 40 tile grid: X's values kept, but those Omega Ruby never uses, mapped (XyTileValue)
//   file 1  the terrain model: BCH 0x07 made 0x21 (BchUpgrade.h), named for its new cell, its textures taken from X's area
//           pack into Omega Ruby's (renamed with the suffix _x<pack> where the pack holds another texture of the same name)
//   file 2  collision ("coll"): kept (the same format, seen)
//   file 3  door and prop models (doors, street lamps, a town map...): their types are X's a/0/2/4 members (X's prop archive, as
//           Omega Ruby's a/0/2/3: read 18 October), listed in the area pack's file 0. The caller appends X's members to a/0/2/3
//           and their entries to the pack (XyPropsToOras), and the types are renumbered for those
//   file 4  the piece's cell, row and column in its first two bytes: written for the new cell (a stale one froze the game,
//           ORAS_LITTLEROOT.md section 2b)
//   file 5  tall grass: a BCH upgraded as file 1, 128 zero bytes kept
//   file 6  Omega Ruby's alone ("KAGE"): Littleroot's copied

#include "Bytes.h"
#include "Garc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace remake
{

// X's tile value as Omega Ruby reads it: the value itself where Omega Ruby's pieces use it, else its replacement
uint32_t XyTileValue(uint32_t value);

// the door and prop types X's piece places (file 3)
std::set<uint32_t> XyPieceProps(const Bytes& xPiece);

// X's prop types made Omega Ruby's: each X/Y a/0/4/1 member's model (its BM container, BCH 0x07 made 0x21) appended to Omega
// Ruby's a/0/2/3 (orasProps), its textures taken from X's pack into orasAreaPack, and X's entry for it (file 0 of X's pack,
// 0x50 bytes: the type twice, flags, 16 file references) added to orasAreaPack's file 0 under the new member's number.
// Returns X's type -> Omega Ruby's
std::map<uint32_t, uint32_t> XyPropsToOras(const std::set<uint32_t>& types, const Garc& xProps, const Bytes& xAreaPack, size_t xPackIndex,
                                           Garc& orasProps, const Garc& orasPropsOriginal, Bytes& orasAreaPack, std::vector<std::string>& log);

// xPiece: X's a/0/4/1 member (plain); xAreaPack: the X area pack its zone draws from (a/0/1/3 member xPackIndex, plain);
// kage: an Omega Ruby piece's file 6; orasAreaPack: the Omega Ruby pack the piece will draw from, X's textures added to it;
// backgrounds: X/Y's battle backgrounds given Omega Ruby's numbers (OrasXyBackgrounds.h), for the tiles naming one (bits
// 17-23: the background of a battle fought on the tile, FUN_00403190), X's kept without it
Bytes XyPieceToOras(const Bytes& xPiece, const Bytes& xAreaPack, size_t xPackIndex, const Bytes& kage, const std::string& modelName,
                    int cellX, int cellY, const std::map<uint32_t, uint32_t>& propTypes, Bytes& orasAreaPack, std::vector<std::string>& log,
                    struct XyBackgroundNumbers* backgrounds = nullptr);

}

#endif // REMAKE_ORASXYPIECE_H
