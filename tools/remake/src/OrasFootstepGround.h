#ifndef REMAKE_ORASFOOTSTEPGROUND_H
#define REMAKE_ORASFOOTSTEPGROUND_H

// Footstep grounds from the textures (owner's request, 30 October). A step's sound comes from the ground type of the tile
// under the foot (tile word bits 12-16 in Omega Ruby, OrasXyFootsteps.h), and both games leave almost every tile at ground
// 0, which has no sound (96% of Omega Ruby's tiles; of the walked ones, the grass, paths and floors indoors). Each walked
// tile at ground 0 is given the surface it is drawn as: the texture of the highest upward triangle over the tile's centre
// in the piece's terrain model, matched by name against a table of the two games' texture names, to one of the surfaces
// OrasXyFootsteps.h plays (soft grass, dirt, paving, wood, forest floor, cave rock, carpet, indoor floor) or a ground the
// games have (sand, shallows, metal, X's snow). A tile whose texture matches nothing stays at 0, silent as before; so do
// the rooms whose model is placed by its skeleton (not applied here: 84 of Omega Ruby's pieces show no face over their
// tiles).

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

enum class FootstepGame { Oras, X };

// the texture each of the piece's 40 x 40 tiles shows from above (row-major as the tile block, "" none), from a
// decompressed GR piece
std::vector<std::string> TileSurfaceTextures(const Bytes& gr);

// the ground a texture name walks as in that game's footstep table (a surface, FootstepSurface, or the game's own), 0 for none
int GroundOfTexture(const std::string& texture, FootstepGame game);

// the piece with its walked ground-0 tiles given their texture's ground; counts the tiles changed
Bytes WithTextureGrounds(const Bytes& gr, FootstepGame game, int* changed = nullptr);

}

#endif // REMAKE_ORASFOOTSTEPGROUND_H
