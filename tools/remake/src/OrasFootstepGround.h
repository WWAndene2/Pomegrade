#ifndef REMAKE_ORASFOOTSTEPGROUND_H
#define REMAKE_ORASFOOTSTEPGROUND_H

// Footstep grounds from the textures (owner's request, 30 October). A step's sound comes from the ground type of the tile
// under the foot (tile word bits 12-16 in Omega Ruby, OrasXyFootsteps.h), and both games leave almost every tile at ground
// 0, which has no sound (96% of Omega Ruby's tiles: grass, paths, floors indoors). Each tile at ground 0 is given the
// ground its surface is drawn as: the texture of the highest walkable triangle over the tile's centre in the piece's
// terrain model, matched by name against a table of the two games' texture names. A tile whose texture matches nothing
// stays at 0, silent as before.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

enum class FootstepGame { Oras, X };

// the texture each of the piece's 40 x 40 tiles shows from above (row-major as the tile block, "" none), from a
// decompressed GR piece
std::vector<std::string> TileSurfaceTextures(const Bytes& gr);

// the ground a texture name walks as in that game's footstep table, 0 for none known
int GroundOfTexture(const std::string& texture, FootstepGame game);

// the piece with its ground-0 tiles given their texture's ground; counts the tiles changed
Bytes WithTextureGrounds(const Bytes& gr, FootstepGame game, int* changed = nullptr);

}

#endif // REMAKE_ORASFOOTSTEPGROUND_H
