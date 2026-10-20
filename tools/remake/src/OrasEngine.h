#ifndef REMAKE_ORASENGINE_H
#define REMAKE_ORASENGINE_H

// The engine patches lifting Omega Ruby's limits (ORAS_ENGINE.md 4), gathered into the one exheader.bin and exefs/code.ips a
// mod can carry (Azahar reads a single code.ips per game): the application memory and linear heap (OrasMemory.h), the zone
// count's code side (section 2: the header loaders' bound and the zone header table's size word) and the size of any
// sub-heap made at boot by FUN_00107c0c (section 4.2). Every word is checked against the game's own before it is written.
// The character-model limit also changes the field module DllField.cro (a BPS patch of it: Azahar only, see OrasEngine.cpp).

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
    std::optional<uint32_t> Characters;      // characters a zone may list and show (the game's 26; up to 255; above 31 save block 10 holds N + 1)
    std::optional<uint32_t> CharacterModels; // models an area pack's character list (a/1/3/7) may hold (the game's 31; 32-127)
    bool LargeEvents = false;                // a zone's events file up to 0x1910 bytes (the game's 0xC84)
    bool LargePacks = false;                 // area packs past 255 (the zone change cut the pack number to a byte)
    bool LargeWildTables = false;            // X/Y's land tables: kinds 9-12 (tiles 0x21-0x23 and the kinds Omega Ruby has none for)
    bool LargeStory = false;                 // story flags 0xD00-0x2C3F and event works 0x4178-0x436B, in save block 47
    bool XyTrainers = false;                 // X/Y's trainers 950-1734 (OrasXyTrainers.h): the trainer bounds, their flags 0x2900 + n
    std::optional<std::pair<uint32_t, uint32_t>> XySounds; // X/Y's streams appended to the sound archive: the first's index, their count
    std::map<uint32_t, uint32_t> HeapSizes;  // heap id -> size (bytes)
    std::map<uint32_t, uint32_t> ScriptMasks; // zone 536-1023 -> its script's native mask (with ZoneRows; 0x243 otherwise)
    bool XyMessages = false;                 // MsgLoad(table, 0x100 + m): member m of table 8 (X/Y's message files appended)
    std::optional<uint32_t> StartZone;       // the zone a new game starts in, at its header's spawn (the game's 399, the truck)
    std::string XyGyms;                      // X/Y's image: its gym modules ported and loaded by their area kinds (OrasXyGymModules.h)
};

// the ARM data-processing immediate encoding of value (an 8-bit value rotated right by an even amount), when there is one
std::optional<uint32_t> ArmImmediate(uint32_t value);

// writes load/mods/<program id>/exheader.bin (when Memory is given), exefs/code.ips and, with CharacterModels, the patch of
// the field module romfs_ext/DllField.cro.bps under outDir; returns what it did
std::vector<std::string> BuildEngineMod(N3dsRom& oras, const OrasEngineOptions& options, const std::string& outDir);

}

#endif // REMAKE_ORASENGINE_H
