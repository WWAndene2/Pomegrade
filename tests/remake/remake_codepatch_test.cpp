// Patches to the game's code (CodePatch.h): the IPS Azahar applies to the decompressed code, written only over the words
// the game ships, applied back as Azahar does; the extended header's memory mode (OrasMemory.h); and the save layout
// oras-engine --characters N > 31 gives the game (OrasSave::ResizeMoveModels), on a save built here
#include "CodePatch.h"
#include "OrasEngine.h"
#include "OrasMemory.h"
#include "OrasSave.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
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
    const Bytes newer = ExHeaderWithSystemMode(exheader, OrasMemoryMode::New124);
    Bytes rest124 = newer;
    rest124[0x20D] = 0;
    check(newer[0x20D] == 1 && newer[0x20E] == 0x04 && rest124 == exheader, "the New 3DS mode written as n3ds_mode 1, the system mode kept");
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

    // a save of the game's layout: blocks 0-10 of the game's sizes, an 11th filling up to the footer at 0x75E00, the table at
    // 0x75E14 after FEEB; each block's bytes its index, and block 10's records numbered
    const size_t sizes[] = {712, 2960, 44, 56, 336, 4, 8, 448, 190, 36, 0x2100, 0x71C00};
    Bytes main(0x76000, 0);
    size_t offset = 0;
    for (size_t i = 0; i < 12; i++)
    {
        std::fill(main.begin() + offset, main.begin() + offset + sizes[i], (uint8_t)(i + 1));
        for (int k = 0; k < 4; k++) main[0x75E14 + 8 * i + k] = (uint8_t)(sizes[i] >> (8 * k));
        main[0x75E14 + 8 * i + 4] = (uint8_t)i;
        offset = (offset + sizes[i] + 0x1FF) & ~(size_t)0x1FF;
    }
    for (size_t r = 0; r < 32; r++) main[0x2000 + r * OrasMoveModelRecord] = (uint8_t)(0x80 + r);
    std::memcpy(&main[0x75E10], "FEEB", 4);
    main[0x75E00] = 0x5A; // the footer's save id
    OrasSave save;
    save.Data = main;
    for (size_t i = 0, at = 0; i < 12; at = (at + sizes[i] + 0x1FF) & ~(size_t)0x1FF, i++) save.Blocks.push_back({at, sizes[i], (uint16_t)i, 0});
    save.WriteChecksums();
    bool read = true;
    try { save = OrasSave::Read(save.Data); } catch (const FormatError&) { read = false; }
    check(read && save.Blocks.size() == 12 && save.TableAt() == 0x75E14, "the built save is read: 12 blocks, the table at 0x75E14");
    check(OrasMoveModelBytes(32) == 0x2100 && OrasMoveModelBytes(73) == 0x4C00 && OrasMoveModelBytes(65) == 0x4400 && ArmImmediate(0x4C00),
          "block 10's size: 0x2100 for the game's 32 records, 73 records 0x4B48 bytes rounded up to the immediate 0x4C00");
    OrasSave grown = save;
    grown.ResizeMoveModels(73);
    bool reread = true;
    try { grown = OrasSave::Read(grown.Data); } catch (const FormatError&) { reread = false; }
    const OrasSaveBlock& b10 = grown.Blocks[10], &b11 = grown.Blocks[11];
    check(reread && grown.Data.size() == 0x76000 + 0x2A00 && b10.Offset == 0x2000 && b10.Length == 0x4C00 && b11.Offset == 0x6C00,
          "grown to 73 records: block 10 0x4C00 bytes, block 11 moved to 0x6C00, the file 0x2A00 bytes longer, checksums right");
    bool kept = std::equal(main.begin(), main.begin() + 0x4100, grown.Data.begin()) &&
                std::equal(main.begin() + 0x4200, main.begin() + 0x4200 + 0x71C00, grown.Data.begin() + 0x6C00) &&
                std::all_of(grown.Data.begin() + 0x4100, grown.Data.begin() + 0x6C00, [](uint8_t v) { return v == 0; }) &&
                grown.Data[grown.Data.size() - 0x200] == 0x5A;
    check(kept, "blocks 0-10 and the 32 records kept, the new records zero, block 11 and the footer moved whole");
    OrasSave back = grown;
    back.ResizeMoveModels(32);
    check(back.Data == save.Data, "back to 32 records: the very bytes of the game's save");
    bool cutRefused = false;
    Bytes cut = grown.Data;
    cut.resize(0x76000);
    try { OrasSave::Read(cut); } catch (const FormatError&) { cutRefused = true; }
    check(cutRefused, "a grown save cut to the game's size is refused");

    printf(ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
