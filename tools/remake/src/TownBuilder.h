#ifndef REMAKE_TOWNBUILDER_H
#define REMAKE_TOWNBUILDER_H

// An ORAS map piece for a town, built from a TownLayout (what a Platinum window
// is made of) with ORAS's own assets: the houses, trees, flowers, hedges, paths
// and pond are ORAS models and materials, placed where Platinum has them. The
// collision block gets Platinum's collision and the door models its doors.

#include "TownLayout.h"

namespace remake
{

// map pieces ("GR" containers of a/0/3/9), decompressed
struct TownSources
{
    Bytes Target; // the piece to overwrite: its layout, tile block and door models are rewritten
    Bytes Donor;  // the piece whose terrain materials and models the town is made of
    Bytes Trees;  // the piece the trees come from
    int CellX = 0, CellY = 0; // the target piece's cell in its map matrix (its model name's two numbers)
};

// the new piece's bytes; log: what was placed
Bytes BuildTown(const TownLayout& layout, const TownSources& sources, std::vector<std::string>* log = nullptr);

}

#endif // REMAKE_TOWNBUILDER_H
