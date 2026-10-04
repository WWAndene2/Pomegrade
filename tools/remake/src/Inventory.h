#ifndef REMAKE_INVENTORY_H
#define REMAKE_INVENTORY_H

// Everything a DS cartridge holds, as JSON: each file's path, size and kind,
// and for archives (NARC) their members' kinds, after decompressing LZ data.
// The first map of what to convert; nothing of the game's data is copied
// into it.

#include "NdsRom.h"

#include <string>

namespace remake
{

std::string InventoryJson(const NdsRom& rom);
// the kind of data, decompressing LZ first: "nsbmd", or "lz10:nsbmd"
std::string DeepKind(const Bytes& data);

}

#endif // REMAKE_INVENTORY_H
