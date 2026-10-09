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
    // the texture Platinum's snow patches show (empty: they are the lighter grass), and the one its fences show (empty: a hedge);
    // both must be in the area pack too
    std::string SnowTexture, FenceTexture;
    // the snow's edge clumps (empty: none): chip_alpha's stone cluster filled with the snow, made by OrasTown
    std::string SnowClumpTexture;
    // an interior's floor (empty: none): every ground mesh (grass, paths, the lighter grass) shows it, an ORAS house's floor in
    // place of the outdoor ground (oras-region on an interior's matrix, SINNOH_BUILD.md R4); it must be in the area pack
    std::string FloorTexture;
    // the pond's inner walls (empty: the donor's cliff band): their texture and its rows at the top and the foot of the wall
    std::string BankTexture;
    float BankV[2] = {0, 0};
    // trees are placed up to this many tiles from open ground (-1: none). Twinleaf with 2 is the largest piece seen to show on the
    // phone (996,992 bytes); with 3 (1,074,944 bytes) the map showed nothing
    int TreeReach = 2;
    // the door models' type (0: the target's own, entry by entry); their other words stay the target's
    uint32_t DoorType = 0;
    // the bottom row and the side columns made solid: a town built alone in another place's cell (oras-town); oras-region's pieces
    // meet their neighbours there
    bool CloseEdges = true;
    // the terrain model's name, which tells its place (world<matrix>_<x>_<y>, 13 characters as the game's); empty: the target's
    std::string ModelName;
    // ledges: the layout's 'L' tiles a ledge jumped down southward (oras-sandbox; a region keeps Platinum's ledges as their
    // permissions give them): the tile value Route 101's south ledges have (0x75000021, piece 5, rows 15, 16, 28) and a one-tile
    // slice of Route 101's ledge (mesh 7, chip_jump_gake9, tile 19 of row 28) on each, in Petalburg's snow-band mesh, which then
    // shows chip_jump_gake (area pack 8)
    bool Ledges = false;
};

// the new piece's bytes; log: what was placed
Bytes BuildTown(const TownLayout& layout, const TownSources& sources, std::vector<std::string>* log = nullptr);

}

#endif // REMAKE_TOWNBUILDER_H
