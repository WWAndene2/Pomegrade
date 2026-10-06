// Game text files (GameText.h): lines written and read back, the key schedule checked on a unit computed here by hand
// (line 1's first unit is XORed with 0x7C89 + 0x2983), variables and escapes kept. No game data.
#include "GameText.h"

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
    const std::vector<std::string> lines = {"Bourg-Geon", "Hello [VAR 0100(0001)]!\\nSecond line", "é\\xE07F[VAR 0201()]", ""};
    const Bytes file = WriteGameText(lines);
    check(U16(file, 0) == 1 && U16(file, 2) == 4, "one section, four lines");
    const uint32_t section = U32(file, 12);
    const uint32_t line1 = U32(file, section + 4 + 8);
    check(U16(file, section + line1) == ('H' ^ (uint16_t)(0x7C89 + 0x2983)), "line 1 keyed from 0x7C89 + 0x2983");
    const uint16_t k0 = 0x7C89, k1 = (uint16_t)((k0 << 3) | (k0 >> 13));
    check(U16(file, section + U32(file, section + 4) + 2) == ('o' ^ k1), "the key rotates left by 3 after each unit");
    check(ReadGameText(file) == lines, "lines read back as written, variables and escapes kept");
    check(U16(file, section + 4 + 8 * 3 + 4) == 1, "an empty line holds its terminator");
    bool threw = false;
    try { WriteGameText({std::string(70000, 'a')}); } catch (const FormatError&) { threw = true; }
    check(threw, "a line over 65535 units refused, not truncated");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
