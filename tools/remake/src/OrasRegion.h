#ifndef REMAKE_ORASREGION_H
#define REMAKE_ORASREGION_H

// A region of Platinum rebuilt as a new ORAS map matrix: a rectangle of Sinnoh's piece grid, cut as `oras-world` cuts it
// (ORAS piece (X, Y) holds Platinum's matrix tiles 40X..40X+39, 40Y..40Y+39), every piece built as `oras-town` builds one
// (BuildTownPiece) and appended to a/0/3/9, a matrix appended to a/0/4/0 holding them with its zone grid filled from Platinum's
// map headers, and the ORAS zones that stand for those headers moved onto it with their warps on Platinum's doors.
//
// Zones cannot be appended (ORAS_LITTLEROOT.md 11, a4), so each Platinum header used is given an existing ORAS zone number
// (Zones); that zone keeps its own area pack (rule 6), which receives the textures of the pieces it lies on. What else the
// reused zone holds (characters, furniture, triggers, scripts: Hoenn's) is kept and reported, not rebuilt.
//
// Global grid, not a window per place: Sinnoh is rebuilt matrix by matrix out of one grid, so neighbouring matrices meet without
// re-cutting (the owner: long term, Sinnoh as Platinum lays it out).

#include "N3dsRom.h"
#include "NdsRom.h"
#include "OrasTown.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

// a door of the region, in matrix tiles, with its Platinum destination header and the ORAS zone it leads into (-1: none of its own)
struct RegionDoor { int X = 0, Y = 0, DestHeader = 0, Interior = -1; };

struct OrasRegionOptions
{
    // the kits, textures and design rules of each piece, as oras-town takes them; its window, cell, zone and area pack are not used
    OrasTownOptions Town;
    int Left = 0, Top = 0, Width = 0, Height = 0;   // the rectangle, in ORAS pieces of Sinnoh's grid
    // Platinum map header -> ORAS zone (a/0/1/3 member); -1: the header's blocks are left out of the matrix (0xFFFF). A header with
    // no block in the rectangle and a door leading to it (a house) is that door's interior: the door's warp leads into the zone,
    // whose warp 0 leads back out to the door; one zone per door
    std::map<int, int> Zones;
    bool OthersOut = false;     // a header Zones does not name is left out, as with -1 (a large matrix holding a few places)
    size_t MatrixTemplate = 1;  // the matrix whose file 1 (meaning unknown) the new matrix copies: Littleroot's
    int ModelMatrix = 15;       // the pieces' model names, world<NN>_<x>_<y> (the game's run from world01 to world14)
    bool PlanOnly = false;      // print the rectangle's map headers, block by block, and build nothing
    std::string OutDir;
};

// writes <OutDir>/load/mods/<program>/romfs_ext/{a/0/3/9,a/0/4/0,a/0/1/3,a/0/1/4}.bps, region_preview.gltf and region_plan.txt;
// returns what it did, line by line
std::vector<std::string> BuildOrasRegion(const NdsRom& platinum, N3dsRom& oras, const OrasRegionOptions& options);

// The plan for the whole of Sinnoh as oras-region builds it, nothing written: Sinnoh's piece grid cut into strips of `stripWidth`
// columns (each a matrix, trimmed to its used rows), the oras-region rectangle and map headers of each, the edge warps between
// neighbouring strips, and how many ORAS zones the headers need against the ones that can be reused
std::vector<std::string> PlanSinnoh(const NdsRom& platinum, N3dsRom& oras, int stripWidth);

}

#endif // REMAKE_ORASREGION_H
