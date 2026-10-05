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
    // the ground is the target piece's own grass (its chip_kusa_a and chip_kusa_b materials' vertex colours) instead of the
    // donor's: the donor's area pack must then hold that grass's pixels under the donor's grass texture names (OrasTown does this)
    bool TargetGrass = false;
    // the textures the ground's materials show, when they are not the donor's (empty: kept): the main grass, the lighter grass, and the
    // grass edge (the rim's second texture slot). They must be in the area pack: OrasTown adds them there.
    std::string GroundTexture, LightTexture, EdgeTexture;
    // the texture Platinum's snow patches show (empty: they are the lighter grass); it must be in the area pack too
    std::string SnowTexture;
};

// the new piece's bytes; log: what was placed
Bytes BuildTown(const TownLayout& layout, const TownSources& sources, std::vector<std::string>* log = nullptr);

}

#endif // REMAKE_TOWNBUILDER_H
