// X/Y's dust clouds (OrasXyDustCloud.h): the step check's hook and cave, Route 13's module among the ported ones, its
// archive's number (the room's code needs both games' DllField: checked by oras-engine itself, its layout and words)
#include "OrasXyDustCloud.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t B(uint32_t cond, uint32_t from, uint32_t to) { return cond << 28 | 0x0A000000 | (((to - from - 8) >> 2) & 0xFFFFFF); }

int main()
{
    // the hook: the step check's `ldr r0, [fp, #0x1c]` past its guard a branch to the cave; the cave calls the room's routine
    // through the .code word, leaves through the function's exit with the event, or does the game's word and goes back
    const uint32_t cave = 0x10340002; // aligned up
    const std::vector<CodeWord> f = XyDustCloudFieldPatchAt(cave);
    const auto word = [&](uint32_t a) -> const CodeWord* { for (const CodeWord& w : f) if (w.Address == a) return &w; return nullptr; };
    const uint32_t c = 0x10340004;
    check(word(0x10263304) && word(0x10263304)->Expected == 0xE59B001C && word(0x10263304)->Value == B(0xE, 0x10263304, c), "the step check's word goes to the cave");
    check(word(c) && word(c)->Value == 0xE1A00005 && word(c + 4)->Value == 0xE1A0100B, "the character and the field given");
    check(word(c + 0x24) && word(c + 0x24)->Value == XyDustCloudPointer && word(c + 8)->Value == 0xE59FC014, "the routine's address read from the .code word");
    check(word(c + 0x18) && word(c + 0x18)->Value == B(0x1, c + 0x18, 0x10263498), "an event leaves through the function's exit");
    check(word(c + 0x1C) && word(c + 0x1C)->Value == 0xE59B001C && word(c + 0x20)->Value == B(0xE, c + 0x20, 0x10263308), "else the game's word, and back");
    check(f.size() == 11, "the hook and ten cave words (the effect pack grown by OrasXyEffectPack.h)");

    // Route 13's module: X's area kind 0x18 loads it as the mod's 18, its archive the third number past the game's
    check(XyGymAreaKind(0x18) == 18, "X's area kind 0x18 is the mod's 18");
    check(XyRoute13().XName == "DllUSR13" && XyRoute13().Name.size() == XyRoute13().XName.size(), "the module renamed at the same length");
    check(XyRoute13().Archive == 0xAF && XyGymArchiveNumberOf(XyRoute13()) == 0x150, "the effect's archive a/1/7/5 gets 0x150 (after the Ice and Elec gyms')");
    check(XyRoute13().Classes.size() == 1 && XyRoute13().Classes[0].Constructor == 0x938 && XyRoute13().Classes[0].ObjectSize == 0x70, "its class: X's constructor and size");
    check(XyDustCloudBattleTypeNumber == 95, "X/Y's battle type 0x16 is the mod's 95");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
