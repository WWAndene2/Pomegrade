#ifndef REMAKE_AMX_H
#define REMAKE_AMX_H

// The header of a Pawn (AMX) script, which is what ORAS's zone scripts are (magic 0xF1E0, versions 10/10, the "compact"
// flag 0x04 set: the code is stored packed). The header is the standard AMX one; only it is read here, not the code.
// Measured on Littleroot's zone script: 6438 bytes holding 10432 bytes of code, 684 of data, 1 public, 58 natives.

#include "Bytes.h"

#include <string>
#include <utility>
#include <vector>

namespace remake
{

struct AmxInfo
{
    uint32_t Size = 0;       // of the whole script
    uint16_t Magic = 0, Flags = 0, DefSize = 0;
    uint8_t FileVersion = 0, AmxVersion = 0;
    uint32_t Cod = 0, Dat = 0, Hea = 0, Stp = 0, Cip = 0, Publics = 0, Natives = 0, Libraries = 0;
    uint32_t CodeBytes() const { return Dat - Cod; }   // once unpacked
    uint32_t DataBytes() const { return Hea - Dat; }
    uint32_t PublicCount() const { return DefSize ? (Natives - Publics) / DefSize : 0; }
    uint32_t NativeCount() const { return DefSize ? (Libraries - Natives) / DefSize : 0; }
    bool Compact() const { return Flags & 0x04; }

    // throws FormatError when the bytes are not an AMX script
    static AmxInfo Read(const Bytes& script);
};

// The code and data cells of a script, unpacked when it is compact. Pawn's compact encoding (amx.c, expand): each cell
// is written as 7-bit groups, most significant first, every byte but a cell's last with bit 7 set; bit 6 of a cell's
// first byte extends its sign. Unpacked from Cod, (Hea - Cod) / 4 cells; throws when the packed bytes do not end at
// the script's size, the check that the encoding is the one the game uses.
std::vector<int32_t> AmxCells(const Bytes& script);

// The names of a script's publics or natives (the tables at Publics and Natives: an address and a name offset each)
std::vector<std::pair<uint32_t, std::string>> AmxPublics(const Bytes& script);
std::vector<std::string> AmxNatives(const Bytes& script);

// The code disassembled with Pawn 3's opcode numbers (amx.h): the game's version (10/10) may number them otherwise,
// so the listing ends with the count of cells that are no known opcode, the check of that table on a real script
std::string AmxDisassemble(const Bytes& script);

}

#endif // REMAKE_AMX_H
