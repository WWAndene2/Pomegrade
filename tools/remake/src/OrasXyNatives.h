#ifndef REMAKE_ORASXYNATIVES_H
#define REMAKE_ORASXYNATIVES_H

// X/Y's own script natives (the ones Omega Ruby has none of) given to Omega Ruby as new engine code, under X/Y's names, so
// that X/Y's zone scripts call them unchanged. Read 20 October.
//
// How a script finds its natives: two functions link them, each passing {char *name, function} tables to
// Sys_LinkImportsByNameHash (0x506190), which hashes each name and copies the functions into the script's import slots (the
// table is read once, not kept: read in 0x506190): Script_RegisterNativeTablesByMask (0x3FBC80) one table per bit of the
// script's mask, Script_LinkNativeImports (0x506118) a fixed set (bits 1, 0x40, 4, 0x80, 0x400; zone 538's init script,
// X/Y 0's, is linked this way: its IECreate, bit 4, was linked and its IsZoneWalk was not, heap dump of run z538l). Both call
// the X/Y natives' registration before their last table: the first's bit-0x400 test (0x3FBDA0) and the second's call of the
// bit-0x400 getter (0x506178) become calls of two stubs in the .code's free bytes (0x579E80-0x579EC7; the other options'
// ranges: OrasEngine.cpp, and A's log of 20 October), which do what they replace after it. A script whose imports are not all
// resolved is relinked by amx_Exec (0x503320) and not run: zone 538 stayed black while IsZoneWalk was unresolved (runs
// z538b-j, 20 October). They register them for every script, whatever its mask: Omega Ruby's own scripts import none of
// these X/Y names. Bit 0x800 (XyNativesBit) stays the sign, in a zone's mask, that the mod needs them (oras-engine installs
// them then).
//
// The natives' code, names, the table of their offsets and the registration subroutine live in DllField's cave (the bytes
// past its code segment, OrasEngine.cpp), shared with the other DllField patches: it is executable, and DllField is where the
// game's own natives are. DllField is mapped where the heap has room, so the stubs find it through the .code's import stub to
// the getter of DllField's bit-2 table (0x19C8D0: `ldr pc, [pc, #-4]` and the getter's address, written by the loader at
// 0x19C8D4; the getter at 0x1029F1E0 in the linked program of prototype/cro_link.py), in DllField's code segment like the
// cave; not from the table it returns, as a module's data segment is loaded apart from its code. The subroutine finds the
// table from its own pc, and the table's offsets are from itself. The natives call the .code only (absolute addresses,
// `ldr ip, =f; blx ip`), so they run wherever DllField lies. Azahar only, as every DllField patch (a console checks the
// module's hash).

#include "CodePatch.h"

#include <functional>
#include <string>
#include <vector>

namespace remake
{

constexpr uint32_t XyNativesBit = 0x800; // the zone mask bit registering them

struct XyNative
{
    std::string Name;            // X/Y's name (its scripts call it by this name's hash)
    std::vector<uint32_t> Code;  // ARM, position-independent: it may only branch inside itself and call absolute addresses
};

// every X/Y native the mod provides (OrasXyNickname.h ...)
const std::vector<XyNative>& XyNatives();

// AmxNativeCheck(namesFile, mask) knowing XyNatives() as registered under XyNativesBit
std::function<std::string(const std::string&)> XyNativeCheck(const std::string& namesFile, uint32_t mask);

// the words installing them: Code in the .code (code.ips), Field in DllField at its linked addresses from fieldCave (the
// first free word of its cave), every one over a zero but the branch at 0x3FBDA0
struct XyNativesPatch
{
    std::vector<CodeWord> Code, Field;
};
XyNativesPatch XyNativesPatchAt(uint32_t fieldCave);

// an ARM call of an absolute address (`ldr ip, [pc, #k]; blx ip`, its address in a pool after the code) and branches
// inside the code being written, for the natives' code
class XyNativeAsm
{
public:
    void Op(uint32_t word) { Words.push_back(word); }
    void Call(uint32_t address);
    // a call of a function of DllField by its address in the linked program (prototype/cro_link.py), through the getter's
    // address the loader writes in the .code (DllField is mapped where the heap has room): r12 and lr are used
    void CallField(uint32_t linked);
    // `ldr rd, [pc, #k]`, the value in the pool
    void Literal(uint32_t rd, uint32_t value);
    // a branch (cond: the top 4 bits, 0xE always, 0x0 eq ...) to a label set later
    void Branch(uint32_t cond, int label);
    int NewLabel();
    void Set(int label);
    std::vector<uint32_t> Finish(); // the code and its pool
private:
    std::vector<uint32_t> Words;
    struct PoolLoad { size_t At; uint32_t Rd, Value; };
    std::vector<PoolLoad> Pool;
    std::vector<std::pair<size_t, int>> Branches;
    std::vector<int> Labels;
};

}

#endif // REMAKE_ORASXYNATIVES_H
