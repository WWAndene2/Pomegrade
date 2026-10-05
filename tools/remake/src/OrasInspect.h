#ifndef REMAKE_ORASINSPECT_H
#define REMAKE_ORASINSPECT_H

// What an ORAS zone, map piece or area pack is made of, as text: every part with its size and what is understood of
// it, so nothing has to be learnt by hand-dumping bytes. The layouts it relies on are the ones documented in
// OrasZone.h and tools/remake/ORAS_LITTLEROOT.md, each checked on the real files.

#include "N3dsRom.h"

#include <string>

namespace remake
{

std::string InspectZone(N3dsRom& game, size_t zone);   // a/0/1/3 member: header, entities, scripts
std::string InspectPiece(N3dsRom& game, size_t piece); // a/0/3/9 member: tiles, terrain model, collision, doors
std::string InspectArea(N3dsRom& game, size_t area);   // a/0/1/4 member: its files and textures
// a/0/4/0 member: its files' u16 words as they are (the map matrix's layout is unknown: this is what decodes it)
std::string InspectMatrix(N3dsRom& game, size_t matrix);
// every a/0/3/9 piece's terrain model name (world<matrix>_<x>_<y> for the overworld's): "index name" lines
std::string InspectPieceNames(N3dsRom& game);
// every a/0/1/3 zone in one line: its matrix and area pack, entity counts, its warps' destinations and tiles, its triggers' tiles
std::string InspectZones(N3dsRom& game);
// every a/0/4/0 matrix checked against the layout of ORAS_LITTLEROOT.md 2b, and every zone entity against the zone grid
std::string InspectMatrices(N3dsRom& game);
// every RomFS file: GARC member count, size and the first member's first bytes (what a zone number or a piece number may index)
std::string InspectArchives(N3dsRom& game);


// Conformity of the tooling with the real game, run on the owner's dump: every texture file of every area pack rewritten by
// BchWriteTextureFile (sections before the relocations identical, relocations equal as a set), every zone and its scripts read by
// OrasZone, every map piece's terrain read. Returns the report; ok is false when anything fails.
std::string VerifyGame(N3dsRom& game, bool& ok);

// packs.tsv (each area pack's textures) and pieces.tsv (each map piece's size, model, and per mesh its material, textures and triangles)
// written to a directory: the index to look assets up in, instead of opening 857 pieces by hand
std::string CatalogGame(N3dsRom& game, const std::string& directory);

}

#endif // REMAKE_ORASINSPECT_H
