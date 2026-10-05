#ifndef REMAKE_TOPVIEW_H
#define REMAKE_TOPVIEW_H

// A map piece seen from above, simplified: every mesh is flat-coloured by what its material is (effects, cliffs, forest floor, ground, lighter
// grass, path, shadows, water, structures, grass blades, forest rim, other), drawn in that order. Optional overlays: the
// tile grid (a line every tile, stronger every 5), each tile's value from the piece's tile block (a colour per value),
// and the doors. It shows geometry the way the zone shapes are built, which a textured preview hides; the same
// image can be made from the game's piece or from a mod's.

#include "Bch.h"

namespace remake
{

enum class TopViewKind { Effect, Cliff, Forest, Ground, Light, Path, Shadow, Water, Structure, Blades, Rim, Other };

struct TopViewOptions
{
    int PixelsPerTile = 24;
    bool Grid = false;   // lines on the 18-unit tile lattice
    bool Tiles = false;  // a colour per tile value of the tile block
    bool Doors = false;  // the door models' tiles
};

// the kind a mesh is drawn as, from its material's name and first textures
TopViewKind ClassifyMesh(const BchMaterial& material);
// the same for a mesh of a model: a blended structure mesh (layer above 0) is a cover over the ground, so an effect
TopViewKind MeshKind(const BchModel& model, const BchMesh& mesh);

// the picture as RGBA8, 40 * PixelsPerTile square (what the PNG holds)
Bytes RenderTopViewRgba(const BchModel& model, const Bytes& tileBlock, const Bytes& doorBlock, const TopViewOptions& options);

// a PNG of the 40 x 40 tile piece; tileBlock and doorBlock are GR parts 0 and 3 (may be empty when no overlay is asked)
Bytes RenderTopView(const BchModel& model, const Bytes& tileBlock, const Bytes& doorBlock, const TopViewOptions& options);

// the same from a decompressed GR piece (its terrain model and its tile and door blocks)
Bytes RenderTopViewOfPiece(const Bytes& gr, const TopViewOptions& options);

// RGB of a kind in the picture (a legend)
void TopViewColour(TopViewKind kind, uint8_t rgb[3]);
const char* TopViewName(TopViewKind kind);

}

#endif // REMAKE_TOPVIEW_H
