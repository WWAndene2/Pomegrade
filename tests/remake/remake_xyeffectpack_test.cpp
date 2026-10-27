// The field's effect pack grown to X/Y's effect files (OrasXyEffectPack.h): its builder, allocation and lookup, the skip of
// Omega Ruby's unused ids, and the grass tables' kinds 1-3 naming X/Y's flower grass (the archive itself needs both games)
#include "OrasXyEffectPack.h"
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
    // the swirl's files alone (the dust clouds): ids 8-0xBF
    const uint32_t s = 0x10340004;
    std::vector<CodeWord> f = XyEffectPackPatchAt(0x10340002, false); // aligned up
    const auto word = [&](uint32_t a) -> const CodeWord* { for (const CodeWord& w : f) if (w.Address == a) return &w; return nullptr; };
    check(f.size() == 9, "the effect pack's three words, its builder's skip (a word and five)");
    check(word(0x1026D680) && word(0x1026D680)->Expected == 0xE3A040A5 && word(0x1026D680)->Value == 0xE3A040B8, "the effect pack: 0xB8 entries");
    check(word(0x1026D7D4) && word(0x1026D7D4)->Expected == 0x3104 && word(0x1026D7D4)->Value == 8 + 0x4C * 0xB8, "its allocation, 0x4C bytes each");
    check(word(0x102AA3AC) && word(0x102AA3AC)->Expected == 0xE35200A4 && word(0x102AA3AC)->Value == 0xE35200B7, "its lookup to id 0xBF");
    check(word(0x1026D6E8) && word(0x1026D6E8)->Expected == 0xE2840008 && word(0x1026D6E8)->Value == B(0xE, 0x1026D6E8, s) + 0x01000000u,
          "the builder's id word calls the skip");
    check(word(s) && word(s)->Value == 0xE2840008 && word(s + 4)->Value == 0xE240C0AD && word(s + 8)->Value == 0xE35C000F,
          "the skip: the game's word, then ids 0xAD-0xBC");
    check(word(s + 0xC) && word(s + 0xC)->Value == 0x812FFF1E && word(s + 0x10)->Value == B(0xE, s + 0x10, 0x1026D79C),
          "other ids back, those to the builder's next entry");

    // with the flower grass: its six files after the swirl's, ids 8-0xC5
    f = XyEffectPackPatchAt(0x10340004, true);
    check(f.size() == 9 && word(0x1026D680)->Value == 0xE3A040BE && word(0x1026D7D4)->Value == 8 + 0x4C * 0xBE, "the effect pack: 0xBE entries");
    check(word(0x102AA3AC)->Value == 0xE35200BD, "its lookup to id 0xC5");

    // the grass tables (DllField + 0xF5C64 models, + 0xF5C7C animations), moved in the file by the code's room
    f = XyFlowerGrassTableWords(0x20000);
    check(f.size() == 6, "kinds 1-3, a model and an animation each");
    const uint32_t models = 0x10337C64 + 0x20000, animations = 0x10337C7C + 0x20000;
    for (uint32_t k = 1; k <= 3; k++)
    {
        const std::string kind = "kind " + std::to_string(k);
        check(word(models + 4 * k) && word(models + 4 * k)->Expected == 0x2A && word(models + 4 * k)->Value == 0xC0 + 2 * (k - 1), kind + ": grass00 -> X/Y's grass0" + std::to_string(k));
        check(word(animations + 4 * k) && word(animations + 4 * k)->Expected == 0x29 && word(animations + 4 * k)->Value == 0xC1 + 2 * (k - 1), kind + ": its animation");
    }
    check(!word(models) && !word(models + 16) && !word(animations), "kinds 0 and 4 keep Omega Ruby's");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
