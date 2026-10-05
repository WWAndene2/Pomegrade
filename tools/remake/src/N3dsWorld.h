#ifndef REMAKE_N3DSWORLD_H
#define REMAKE_N3DSWORLD_H

// A DS world (WorldMap) translated to Omega Ruby / Alpha Sapphire's scale and
// map grid. Both games lay their maps out in square tiles, each map piece
// centred on its own origin; measured on the real files:
// - Platinum: 32x32 tiles a piece, a tile 16 units (terrain models 512
//   units wide; buildings' positions in tiles, see LandBuilding);
// - ORAS: 40x40 tiles a piece (the GR files' tile block), a tile 18 units
//   (the collision geometry spans -360..360, its coordinates multiples of
//   18 and 9).
// So a DS tile becomes one ORAS tile, the world's tile grid is cut again
// into 40-tile pieces (the DS world's corner on the ORAS world's corner),
// and every length scales by 18/16 (heights included: one scale for the
// whole world keeps its proportions).
// Not translated here: the tiles' meaning (a DS permission is kept as is:
// ORAS's 32-bit tile values are not decoded yet), characters (DS sprites,
// ORAS models) and cameras.

#include "WorldMap.h"

#include <string>
#include <vector>

namespace remake
{

constexpr float NdsTileUnits = 16.0f;
constexpr float N3dsTileUnits = 18.0f;
constexpr uint32_t N3dsMapTiles = 40;

struct N3dsBuilding
{
    uint32_t Model = 0;      // in the DS area's building archive
    float Position[3] = {};  // ORAS units, from its piece's centre
    size_t SourceCell = 0;   // the DS matrix cell it came from
};

struct N3dsPiece
{
    uint32_t X = 0, Y = 0;
    std::vector<int32_t> Permissions; // N3dsMapTiles^2, row-major: the DS tile's permission, -1 where no DS map is
    std::vector<N3dsBuilding> Buildings;
};

struct N3dsWorld
{
    uint32_t Width = 0, Height = 0; // in ORAS pieces
    float NdsTile = NdsTileUnits;   // the DS world's tile, in its model units
    std::vector<N3dsPiece> Pieces;  // the pieces holding at least one DS tile

    // ndsTile: a DS tile in the DS models' units (the cell size WorldMap measured / 32)
    static N3dsWorld Translate(const WorldMap& world, float ndsTile = NdsTileUnits);
    float Scale() const { return N3dsTileUnits / NdsTile; } // DS model units to ORAS units
    std::string Json() const;
};

}

#endif // REMAKE_N3DSWORLD_H
