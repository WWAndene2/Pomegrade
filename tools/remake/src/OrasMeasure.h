#ifndef REMAKE_ORASMEASURE_H
#define REMAKE_ORASMEASURE_H

// What the whole game says about how its map pieces are built, measured on all of them so a rule is not a rule of Littleroot's
// alone: for every mesh of every piece the shape of its boundary against the 18-unit tile lattice (zones, outlines, rim), the
// heights of the ground, and which surface lies under each tile value of the tile block. Written as TSV files to a directory;
// the returned text is the summary by kind of mesh. The findings are in tools/remake/ORAS_LITTLEROOT.md (9d, 9e).

#include "N3dsRom.h"

#include <string>

namespace remake
{

// zone_shapes.tsv (a row per mesh), tile_surfaces.tsv (a row per tile value), outline_offsets.tsv (a histogram per kind)
std::string MeasureGame(N3dsRom& game, const std::string& directory);

}

#endif // REMAKE_ORASMEASURE_H
