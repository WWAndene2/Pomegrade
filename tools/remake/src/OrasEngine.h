#ifndef REMAKE_ORASENGINE_H
#define REMAKE_ORASENGINE_H

// The engine patches lifting Omega Ruby's limits (ORAS_ENGINE.md 4), gathered into the one exheader.bin and exefs/code.ips a
// mod can carry (Azahar reads a single code.ips per game): the application memory and linear heap (OrasMemory.h), the zone
// count's code side (section 2: the header loaders' bound and the zone header table's size word) and the size of any
// sub-heap made at boot by FUN_00107c0c (section 4.2). Every word is checked against the game's own before it is written.

#include "N3dsRom.h"
#include "OrasMemory.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace remake
{

struct OrasEngineOptions
{
    std::optional<OrasMemoryMode> Memory;   // exheader.bin's system mode
    std::optional<uint32_t> LinearHeap;      // the linear heap FUN_00106448 asks for (bytes)
    std::optional<uint32_t> NormalHeap;      // the normal heap it asks for (bytes); heap 4, carved from its top, grows with it
    std::optional<uint32_t> ZoneRows;        // rows of the zone header table (a/0/1/3 member 536) the mod's archive holds
    std::optional<uint32_t> Characters;      // characters a zone may list and show (the game's 26; at most 32, its pool)
    std::map<uint32_t, uint32_t> HeapSizes;  // heap id -> size (bytes)
};

// the ARM data-processing immediate encoding of value (an 8-bit value rotated right by an even amount), when there is one
std::optional<uint32_t> ArmImmediate(uint32_t value);

// writes load/mods/<program id>/exheader.bin (when Memory is given) and exefs/code.ips under outDir; returns what it did
std::vector<std::string> BuildEngineMod(N3dsRom& oras, const OrasEngineOptions& options, const std::string& outDir);

}

#endif // REMAKE_ORASENGINE_H
