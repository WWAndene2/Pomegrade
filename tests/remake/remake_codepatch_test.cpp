// Patches to the game's code (CodePatch.h): the IPS Azahar applies to the decompressed code, written only over the words
// the game ships, applied back as Azahar does; and the extended header's memory mode (OrasMemory.h)
#include "CodePatch.h"
#include "OrasEngine.h"
#include "OrasMemory.h"
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

    // an extended header as a decrypted dump has it: the program id at the jump id (0x1C8) and the local caps (0x200), flags0
    // at 0x20E (low nibble: processor and affinity, high nibble: system mode)
    Bytes exheader(0x800, 0);
    for (int k = 0; k < 8; k++) exheader[0x1C8 + k] = exheader[0x200 + k] = (uint8_t)(0x11 * k);
    exheader[0x20E] = 0x04;
    const Bytes moded = ExHeaderWithSystemMode(exheader, OrasMemoryMode::Dev1_96);
    check(moded[0x20E] == 0x24, "the system mode set in flags0's high nibble, the low nibble kept");
    Bytes others = moded;
    others[0x20E] = 0x04;
    check(others == exheader, "nothing else changed in the header");
    refused = false;
    Bytes encrypted = exheader;
    encrypted[0x1C8] ^= 0xFF;
    try { ExHeaderWithSystemMode(encrypted, OrasMemoryMode::Dev1_96); } catch (const FormatError&) { refused = true; }
    check(refused, "a header whose jump id and program id differ (encrypted) is refused");

    // ARM immediates, as the game's own instructions encode them (mov r6, #0xDF0000 = E3A068DF; cmp r5, #0x218 = ...0F86)
    check(ArmImmediate(0xDF0000) == 0x8DFu && ArmImmediate(0x218) == 0xF86u && ArmImmediate(0x400) == 0xB01u,
          "immediates encoded as the game's instructions have them");
    // a value with several encodings (0x1800000: 6 ror 10 or 0x18 ror 12) takes the smallest rotation, as assemblers do; any
    // encoding must give the value back
    auto decode = [](uint32_t e) { const uint32_t v = e & 0xFF, r = 2 * (e >> 8); return r ? (v >> r) | (v << (32 - r)) : v; };
    bool roundTrip = true;
    for (uint32_t v : {0x1800000u, 0x200000u, 0x800000u, 0x1000000u, 0x2000000u, 0xF00000u, 0xFD000u})
        roundTrip = roundTrip && ArmImmediate(v) && decode(*ArmImmediate(v)) == v;
    check(roundTrip, "every encoding decodes back to its value");
    check(!ArmImmediate(0x21B) && !ArmImmediate(0x13F0000), "values no 8-bit rotation holds are refused");

    printf(ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
