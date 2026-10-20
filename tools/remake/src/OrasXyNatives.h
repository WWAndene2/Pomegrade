#ifndef REMAKE_ORASXYNATIVES_H
#define REMAKE_ORASXYNATIVES_H

// X/Y's own script natives (the ones Omega Ruby has none of) given to Omega Ruby as new engine code, under X/Y's names, so
// that X/Y's zone scripts call them unchanged. Read 20 October.
//
// How a script finds its natives: Script_RegisterNativeTablesByMask (0x3FBC80) passes one {char *name, function} table per
// bit of the zone's mask to Sys_LinkImportsByNameHash (0x506190), which hashes each name and copies the functions into the
// script's import slots (the table is read once, not kept: read in 0x506190). Bit 0x800 is free (the game tests 0x1-0x400).
// So the test of bit 0x400 (0x3FBDA0) becomes a branch to a routine in the .code's free bytes (0x579E80-0x579EFB; the other
// options' ranges: OrasEngine.cpp, and A's log of 20 October) that, under bit 0x800, builds the table on the stack and
// registers it, then tests bit 0x400 as the game did.
//
// The natives' code, names and the table of their offsets live in DllField's cave (the bytes past its code segment,
// OrasEngine.cpp), shared with the other DllField patches: it is executable, and DllField is where the game's own natives
// are. DllField is mapped where the heap has room, so the routine finds it through the .code's stub to the getter of
// DllField's bit-2 table (0x19C8D0, which returns that table, at 0x10339594 in the linked program of prototype/cro_link.py):
// every offset is from that table. The natives call the .code only (absolute addresses, `ldr ip, =f; blx ip`), so they run
// wherever DllField lies. Azahar only, as every DllField patch (a console checks the module's hash).

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
    // a branch (cond: the top 4 bits, 0xE always, 0x0 eq ...) to a label set later
    void Branch(uint32_t cond, int label);
    int NewLabel();
    void Set(int label);
    std::vector<uint32_t> Finish(); // the code and its pool
private:
    std::vector<uint32_t> Words;
    std::vector<std::pair<size_t, uint32_t>> Calls;
    std::vector<std::pair<size_t, int>> Branches;
    std::vector<int> Labels;
};

}

#endif // REMAKE_ORASXYNATIVES_H
