#ifndef REMAKE_TOWNCHECK_H
#define REMAKE_TOWNCHECK_H

// Design rules a built ORAS map piece must satisfy, as checks the builder runs before it writes a mod. Each rule comes from
// something measured on the real game (tools/remake/ORAS_LITTLEROOT.md) or something that broke on a phone:
//   - every texture a material names must be in the piece's area pack (the pack holds the only textures the game loads
//     for it; a name it lacks draws nothing or garbage);
//   - a piece should stay within the largest original piece's size and vertex count (an untested memory bound: warning);
//   - a mesh's vertices must fit 16-bit indices.
// An error stops the mod being written; a warning is logged.

#include "Bch.h"

#include <set>
#include <string>
#include <vector>

namespace remake
{

struct TownIssue
{
    bool Error = false;
    std::string Text;
};

// the textures `model`'s materials name that are not in `available` (the area pack's texture names; "projection_dummy" and empty names need none)
std::vector<TownIssue> CheckMaterials(const BchModel& model, const std::set<std::string>& available);

// the tile block (GR part 0) and the door block (part 3) of a piece against what the game's 857 pieces do (measured: 208 distinct tile
// values, 94 of them in at least 5 pieces; every door model at scale 1, rotated by a multiple of 90 degrees, at a tile's centre)
std::vector<TownIssue> CheckLayout(const Bytes& tileBlock, const Bytes& doorBlock);

// the tile values the game's pieces use in at least 5 pieces (documented in tools/remake/ORAS_LITTLEROOT.md section 9)
bool TileValueEstablished(uint32_t value);

struct PieceBudget { size_t MaxFileBytes = 0, MaxVertices = 0; }; // the largest of the game's own pieces
// the largest piece seen to show in Littleroot's place on the phone (v24_trees2); 1,074,944 bytes and Littleroot's own piece
// grown to 1,126 KB showed no picture (v24_trees3, t9), although the game's own pieces reach 1,368,064 bytes elsewhere: the
// field's room for a piece is below what other places get, by how much and why is not known
constexpr size_t PieceBytesShown = 996992;
std::vector<TownIssue> CheckBudget(const BchModel& model, size_t fileBytes, const PieceBudget& original);

}

#endif // REMAKE_TOWNCHECK_H
