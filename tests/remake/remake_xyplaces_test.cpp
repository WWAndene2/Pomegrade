// Kalos's places after Omega Ruby's (OrasXyPlaces.h): the six readers' literals pointed at the copy, the eight bounds made
// the new count, and the count's rules (X's table itself needs the game: checked by oras-xy-world)
#include "OrasXyPlaces.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

int main()
{
    const std::vector<CodeWord> w = XyPlacesPatch(0x6B0000, 98);
    const auto word = [&](uint32_t a) -> const CodeWord* { for (const CodeWord& c : w) if (c.Address == a) return &c; return nullptr; };
    check(w.size() == 14, "six literals and eight bounds");
    check(word(0x3E806C) && word(0x3E806C)->Expected == 0x5862E0 && word(0x3E806C)->Value == 0x6B0000, "Zone_SetVisitedFlags reads the copy");
    check(word(0x3E7F60) && word(0x3E7F60)->Expected == 0xE3A0204E && word(0x3E7F60)->Value == 0xE3A02062, "its loop bound: mov r2, #98");
    check(word(0x3D4374) && word(0x3D4374)->Expected == 0xE350004E && word(0x3D4374)->Value == 0xE3500062, "a two-at-a-time loop: cmp r0, #98");
    bool odd = false, small = false;
    try { XyPlacesPatch(0x6B0000, 97); } catch (const FormatError&) { odd = true; }
    try { XyPlacesPatch(0x6B0000, 76); } catch (const FormatError&) { small = true; }
    check(odd && small, "an odd count or one below the game's 78 refused");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
