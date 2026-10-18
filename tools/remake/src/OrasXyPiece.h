#ifndef REMAKE_ORASXYPIECE_H
#define REMAKE_ORASXYPIECE_H

// A Pokemon X/Y map piece (an a/0/4/1 member, a GR container like Omega Ruby's a/0/3/9: ORAS_ENGINE.md 7) made Omega Ruby's,
// for a cell of a new matrix (oras-sandbox's `xy-piece`). X's piece has six files, Omega Ruby's seven:
//   file 0  the 40 x 40 tile grid: X's values kept, but those Omega Ruby never uses, mapped (XyTileValue)
//   file 1  the terrain model: BCH 0x07 made 0x21 (BchUpgrade.h), named for its new cell, its textures taken from X's area
//           pack into Omega Ruby's (renamed with the suffix _x<pack> where the pack holds another texture of the same name)
//   file 2  collision ("coll"): kept (the same format, seen)
//   file 3  door models: left out for now (a zone without warps stops on them; X's door types are not mapped)
//   file 4  the piece's cell, row and column in its first two bytes: written for the new cell (a stale one froze the game,
//           ORAS_LITTLEROOT.md section 2b)
//   file 5  tall grass: a BCH upgraded as file 1, 128 zero bytes kept
//   file 6  Omega Ruby's alone ("KAGE"): Littleroot's copied

#include "Bytes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace remake
{

// X's tile value as Omega Ruby reads it: the value itself where Omega Ruby's pieces use it, else its replacement
uint32_t XyTileValue(uint32_t value);

// xPiece: X's a/0/4/1 member (plain); xAreaPack: the X area pack its zone draws from (a/0/1/3 member xPackIndex, plain);
// kage: an Omega Ruby piece's file 6; orasAreaPack: the Omega Ruby pack the piece will draw from, X's textures added to it
Bytes XyPieceToOras(const Bytes& xPiece, const Bytes& xAreaPack, size_t xPackIndex, const Bytes& kage, const std::string& modelName,
                    int cellX, int cellY, Bytes& orasAreaPack, std::vector<std::string>& log);

}

#endif // REMAKE_ORASXYPIECE_H
