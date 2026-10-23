// X/Y's dust boxes (OrasXyDustBox.h): the block porter on a block made here, the placed prop's routine, the natives
#include "OrasXyDustBox.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void Set(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k)); }
static uint32_t Bl(uint32_t from, uint32_t to) { return 0xEB000000 | (((to - from - 8) >> 2) & 0xFFFFFF); }

int main()
{
    // a block at 0x200000-0x200014: a call out (mapped), a call inside, a literal, a word to change, a plain word
    Bytes code(0x200100 - CodeBase, 0);
    const uint32_t from = 0x200000;
    Set(code, from - CodeBase + 0x00, Bl(from, 0x150000));
    Set(code, from - CodeBase + 0x04, Bl(from + 4, from + 0x10));
    Set(code, from - CodeBase + 0x08, 0x5432A4);
    Set(code, from - CodeBase + 0x0C, 0xE2800F52);
    Set(code, from - CodeBase + 0x10, 0xE12FFF1E);
    ArmBlockPort p;
    p.From = from;
    p.To = from + 0x14;
    p.Calls = {{0x150000, 0x160000}};
    p.Literals = {{0x5432A4, 0x6AF100}};
    p.Words = {{from + 0x0C, {0xE2800F52, 0xE2800F50}}};
    const uint32_t at = 0x6B0000;
    const std::vector<uint32_t> out = PortArmBlock(code, CodeBase, p, at);
    check(out.size() == 5, "the block's five words");
    check(out[0] == Bl(at, 0x160000), "a call out made a call of the mapped function from the new place");
    check(out[1] == Bl(at + 4, at + 0x10), "a call inside kept inside");
    check(out[2] == 0x6AF100 && out[3] == 0xE2800F50 && out[4] == 0xE12FFF1E, "the literal, the changed word, the plain word");
    bool refused = false;
    try { ArmBlockPort q = p; q.Calls.clear(); PortArmBlock(code, CodeBase, q, at); } catch (const FormatError&) { refused = true; }
    check(refused, "a call out not mapped is refused");
    refused = false;
    try { ArmBlockPort q = p; q.Words[from + 0x0C].first = 0; PortArmBlock(code, CodeBase, q, at); } catch (const FormatError&) { refused = true; }
    check(refused, "a word not as expected is refused");

    // the placed prop: the loop's `add r5, r5, #1` a call of the routine, which ends with it
    const uint32_t cave = 0x10340000;
    const std::vector<CodeWord> f = XyDustBoxFieldPatchAt(cave);
    const auto word = [&](uint32_t a) -> const CodeWord* { for (const CodeWord& w : f) if (w.Address == a) return &w; return nullptr; };
    check(word(0x102608AC) && word(0x102608AC)->Expected == 0xE2855001 && word(0x102608AC)->Value == Bl(0x102608AC, cave), "the loop's end calls the routine");
    check(word(cave + 0x54) && word(cave + 0x54)->Value == Bl(cave + 0x54, 0x1031BEAC), "the routine finds the prop of the id");
    check(word(cave + 0x64) && word(cave + 0x64)->Value == Bl(cave + 0x64, 0x10257E34), "and plays its searched animation");
    check(word(cave + 0x6C) && word(cave + 0x6C)->Value == 0xE2855001 && word(cave + 0x74)->Value == 0x12FD48, "the game's word after it; the flag reader in its pool");
    check(word(cave + 0x10) && word(cave + 0x10)->Value == (0x8A000000u | (((0x68u - 0x10 - 8) >> 2) & 0xFFFFFF)), "ids outside 0xC50-0xCE5 skip to the end");

    // the natives: X's names, the battle setting the mod's battle type at the context's + 0x8A
    const std::vector<XyNative> n = XyDustBoxNatives();
    check(n.size() == 3 && n[0].Name == "CallDustBoxEncount" && n[1].Name == "CheckTypeOfDustBox" && n[2].Name == "GetItemIdOfDustBox", "X's three names");
    bool type = false;
    for (size_t k = 0; k + 1 < n[0].Code.size(); k++)
        if (n[0].Code[k] == (0xE3A00000u | XyDustBoxBattleTypeNumber) && n[0].Code[k + 1] == 0xE5C3008A) type = true;
    check(XyDustBoxBattleTypeNumber == 73 + 0x15 && type, "the battle: X/Y's type 0x15 (the mod's 94) at + 0x8A");
    bool pointer = false;
    for (const XyNative& x : n) for (const uint32_t w : x.Code) if (w == XyDustBoxTablePointer) pointer = true;
    check(pointer, "the natives read the table's address from its word");
    return ok ? 0 : 1;
}
