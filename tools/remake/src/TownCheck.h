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
// the largest piece seen to show in an overworld place on the phone: v24 padded to 1,044,480 bytes showed (p1), to 1,052,672 it
// did not (p2; nor 1,074,944, v24_trees3). So an overworld piece gets about 1 MiB (1,048,576), whatever its neighbours (the game's
// own 3 x 3 neighbourhoods reach 4.2 MB); the game's 1.1-1.37 MB pieces are all single-piece places (ORAS_LITTLEROOT.md 10, item 5)
constexpr size_t PieceBytesShown = 1044480;
std::vector<TownIssue> CheckBudget(const BchModel& model, size_t fileBytes, const PieceBudget& original);

}

#endif // REMAKE_TOWNCHECK_H
