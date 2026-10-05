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

}

#endif // REMAKE_ORASINSPECT_H
