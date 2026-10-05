#ifndef REMAKE_WORLDMAP_H
#define REMAKE_WORLDMAP_H

// The world of a DS Pokemon game assembled from its map matrix and land
// data: every map piece placed in its matrix cell, as
// - JSON: each cell's land data, header and height layer, its buildings
//   (model id, position) and its tiles' permissions summary,
// - a collision overview PNG, one pixel per tile (solid tiles dark, the
//   others coloured by behaviour, empty cells grey),
// - one glTF scene: every terrain model placed in its cell, and the
//   buildings when their archive is given.
// A terrain model is centred on its cell; the cell size is measured from
// the terrain models themselves (their widest X extent, rounded to a power
// of two) rather than assumed. The matrix's height layers are reported but
// not applied: each terrain model carries its own heights.

#include "LandData.h"
#include "MapMatrix.h"
#include "Narc.h"
#include "Nsbtx.h"

#include <optional>
#include <string>
#include <vector>

namespace remake
{

// the textures of one matrix cell's area (its map and its buildings)
struct CellTextures
{
    const Tex0* Map = nullptr;
    const Tex0* Buildings = nullptr;
};

struct WorldMap
{
    MapMatrix Matrix;
    std::vector<std::optional<LandData>> Cells; // per matrix cell
    std::vector<std::string> Errors;            // land data that did not read

    WorldMap(MapMatrix matrix, const Narc& lands);

    std::string Json() const;
    Bytes CollisionPng() const;
    // tex: the area's map textures (map pieces usually carry none);
    // buildings: the building models' NARC, by id; buildingTex: their textures;
    // scale: applied to the whole scene (N3dsWorld::Scale for ORAS's); perCell: each cell's own
    // textures (by cell index, over tex and buildingTex where set)
    std::string Gltf(const Tex0* tex, const Narc* buildings = nullptr, const Tex0* buildingTex = nullptr,
                     float* cellSize = nullptr, float scale = 1.0f, const std::vector<CellTextures>* perCell = nullptr) const;
};

}

#endif // REMAKE_WORLDMAP_H
