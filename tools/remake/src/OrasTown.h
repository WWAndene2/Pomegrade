#ifndef REMAKE_ORASTOWN_H
#define REMAKE_ORASTOWN_H

// The whole chain from two game images to an Azahar mod: a Platinum window is
// read (TownLayout), rebuilt as an Omega Ruby map piece (BuildTown), the zone
// that owns the piece gets its warps on the new doors, and both changed archives
// are written as BPS patches (the mod), with a glTF preview and the layout report.

#include "Garc.h"
#include "N3dsRom.h"
#include "NdsRom.h"
#include "TownLayout.h"

#include <string>
#include <vector>

namespace remake
{

// ORAS's own places, as the prototype that the first phone tests used measured them
struct OrasTownOptions
{
    size_t Matrix = 0;                // Platinum's Sinnoh overworld
    int Left = 92, Top = 856;         // the window's first tile (Twinleaf Town)
    size_t TargetPiece = 6, DonorPiece = 8, TreePiece = 5; // a/0/3/9 members: Littleroot, Petalburg, Route 101
    int CellX = 2, CellY = 4;         // the target piece's cell in its ORAS map matrix (Littleroot: world01_02_04)
    size_t Zone = 6;                  // a/0/1/3 member: the zone of the target piece
    size_t AreaPack = 9;              // a/0/1/4 member: the donor's textures, which the new piece names
    // a/0/1/4 member: the target piece's own area (Littleroot's), whose grass the town takes: its pixels go into AreaPack
    // under the donor's grass texture names. -1: the donor's grass is kept
    int GrassPack = 8;
    std::string OutDir;
};

struct OrasTownResult
{
    TownLayout Layout;
    std::vector<std::string> Log;     // what was built, one line each
    size_t PieceBytes = 0;
};

// Member `index` of `archive` replaced by `plain`, LZ-compressed once if the original member was. `plain` must be the
// container itself (its first two letters are `tag`: "GR", "ZO", "AD"): a member that was compressed before this call
// would be compressed twice and the game would read a compressed stream as its data. The member is read back and must
// decompress to exactly `plain`.
void ReplaceMember(Garc& archive, const Garc& original, size_t index, const Bytes& plain, const std::string& tag);

// writes <OutDir>/load/mods/<program>/romfs_ext/{a/0/3/9,a/0/1/3}.bps, town_preview.gltf and town_layout.txt
OrasTownResult BuildOrasTown(const NdsRom& platinum, N3dsRom& oras, const OrasTownOptions& options);

}

#endif // REMAKE_ORASTOWN_H
