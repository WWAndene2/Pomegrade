// Patches to the game's code (CodePatch.h): the IPS Azahar applies to the decompressed code, written only over the words
// the game ships, applied back as Azahar does
#include "CodePatch.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t Word(const Bytes& b, size_t at) { return b[at] | b[at + 1] << 8 | b[at + 2] << 16 | (uint32_t)b[at + 3] << 24; }

int main()
{
    // a code image with two known words: the zone bound's instruction and the table size, at their offsets from 0x100000
    Bytes code(0x300000, 0);
    auto put = [&](uint32_t address, uint32_t v) { for (int k = 0; k < 4; k++) code[address - CodeBase + k] = (uint8_t)(v >> (8 * k)); };
    put(0x2D9774, 0xE3550F86);
    put(0x112C0C, 0x00007540);
    const std::vector<CodeWord> words = {{0x2D9774, 0xE3550F86, 0xE3550B01, "bound"}, {0x112C0C, 0x00007540, 0x000075E8, "size"}};
    const Bytes ips = CodePatchIps(code, words);
    check(ips.size() == 5 + 2 * 9 + 3 && std::string(ips.begin(), ips.begin() + 5) == "PATCH" && std::string(ips.end() - 3, ips.end()) == "EOF",
          "the patch is PATCH, two 4-byte records and EOF");
    const Bytes patched = CodePatchApply(code, ips);
    check(Word(patched, 0x2D9774 - CodeBase) == 0xE3550B01 && Word(patched, 0x112C0C - CodeBase) == 0x000075E8, "applied, both words changed");
    Bytes rest = patched;
    put(0x2D9774, 0xE3550B01); put(0x112C0C, 0x000075E8);
    check(rest == code, "nothing else changed");

    bool refused = false;
    try { CodePatchIps(code, {{0x2D9774, 0xE3550F86, 0, "bound"}}); } catch (const FormatError&) { refused = true; }
    check(refused, "a word that is not the expected one is refused (here already patched)");
    refused = false;
    try { CodePatchIps(code, {{0x10, 0, 0, "below the code"}}); } catch (const FormatError&) { refused = true; }
    check(refused, "an address outside the code is refused");

    printf(ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
