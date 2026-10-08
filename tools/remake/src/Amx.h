#ifndef REMAKE_AMX_H
#define REMAKE_AMX_H

// The header of a Pawn (AMX) script, which is what ORAS's zone scripts are (magic 0xF1E0, versions 10/10, the "compact"
// flag 0x04 set: the code is stored packed). The header is the standard AMX one (AmxInfo); AmxCells unpacks the code and
// data, AmxDisassemble lists the code.
// Measured on Littleroot's zone script: 6438 bytes holding 10432 bytes of code, 684 of data, 1 public, 58 natives.

#include "Bytes.h"

#include <functional>
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

// The code disassembled (the opcode table and how it was checked: Amx.cpp). The listing ends with its own check on the
// script: cells that are no opcode, calls that land on a proc, jumps that land on an instruction.
std::string AmxDisassemble(const Bytes& script);

// The whole script as fields, to write one (SINNOH_BUILD.md R1). Layout of every ORAS zone script (all 536 measured on
// 8 October): Pawn 3.3's header, file version 10 with its overlays word (60 bytes: the overlays word equals the name
// table's offset), then the tables publics, natives, libraries, public variables, tags (8 bytes an entry: an address or
// value, and a name stored as its hash, AmxNameHash), the name table (4 bytes, 0x3F: the longest name), then the code and
// data cells packed (flags 0x1C). Write lays the tables out in that order and packs the cells with Pawn's compact
// encoding; Read then Write gives back the same bytes on every script of the game (`oras-script all`).
struct AmxScript
{
    struct Entry { uint32_t Address = 0, Name = 0; };
    uint8_t FileVersion = 10, AmxVersion = 10;
    uint16_t Flags = 0x1C, DefSize = 8;
    uint32_t Cip = 0, StackBytes = 0;   // where the code starts, and the stack and heap's size (Stp - Hea)
    std::vector<Entry> Publics, Natives, Libraries, PublicVars, Tags;
    Bytes NameTable;
    std::vector<int32_t> Code, Data;

    static AmxScript Read(const Bytes& script);
    Bytes Write() const;
};

// The hash a name is stored and linked by (Script_LinkNativeImports 0x506118: h = h * 0x83 ^ c, c a signed char)
uint32_t AmxNameHash(const std::string& name);

// A script as assembler source that AmxAssemble reads back to the same bytes. Format, one statement a line, ';' starts a
// comment:
//   header <file version> <amx version> <flags> <cip> <stack bytes>     (cip a code label or a number)
//   public|native|library|pubvar|tag <name> [address]   a table entry; <name> a name (hashed) or #XXXXXXXX (the hash);
//            the address a code label (public), a data label (pubvar) or a number; natives may be left undeclared:
//            sysreq.c / sysreq.n NAME declares it on first use, in order
//   nametable <hex bytes>
//   code / data   start the code or data cells; "label:" names the next cell's byte address in either
//   <opcode> <operands>   Amx.cpp's table; a jump or call operand may be a code label (written relative, as the game
//            reads it); packed opcodes (.p.) take their 16-bit operand; casetbl takes its numbers as they are stored
//   cell <n>...   raw cells
// Left out, the header is the game's (10 10 0x1C, cip the label "main" or 0, stack 4096: every zone script's) and the
// library, tag and name table entries are the ones every zone script has ("Float" twice, 0x3F).
std::string AmxSource(const Bytes& script);

// Assembles AmxSource's format; throws FormatError naming the line. `checkNative`, when given, is asked for each native
// named (not given as #hash) and returns why it is refused, or "": a misspelled native, or one the zone's native mask
// does not register, fails here rather than in the game.
Bytes AmxAssemble(const std::string& source, const std::function<std::string(const std::string&)>& checkNative = {});

}

#endif // REMAKE_AMX_H
