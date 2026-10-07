#include "OrasEngine.h"
#include "CodePatch.h"

#include <filesystem>

namespace remake
{

std::optional<uint32_t> ArmImmediate(uint32_t value)
{
    for (uint32_t rot = 0; rot < 16; rot++)
    {
        const uint32_t v = rot ? (value << (2 * rot)) | (value >> (32 - 2 * rot)) : value; // rotated left: the inverse of ror 2*rot
        if (v <= 0xFF) return rot << 8 | v;
    }
    return std::nullopt;
}

// where FUN_00107c0c's call FUN_001120b4(parent, id, size, 0, 0) takes each heap's size (read: ORAS_ENGINE.md 4.2): the
// instruction `mov r2, #size` (Immediate) or the literal-pool word an `ldr r2, [pc, #...]` loads (Word)
struct HeapSize { uint32_t Id, Address, Size; bool Immediate; };
static const HeapSize Heaps[] = {
    {0x8, 0x107D18, 0x20000, true},     {0x9, 0x108570, 0x392000, false},  {0xA, 0x107D48, 0x10000, true},
    {0xB, 0x107D60, 0x1400, true},      {0xD, 0x108574, 0x142420, false},  {0x195, 0x107D94, 0x4000, true},
    {0x112, 0x107DAC, 0x3000, true},    {0x113, 0x107DC4, 0x18000, true},  {0x1DF, 0x107DDC, 0x10000, true},
    {0x1DE, 0x107DF4, 0x3C00, true},    {0x10, 0x107E08, 0x2000, true},    {0x11, 0x107E20, 0x2000, true},
    {0x12, 0x10858C, 0x4110, false},    {0x13, 0x107E50, 0xA800, true},    {0x14, 0x108590, 0x14100, false},
    {0x196, 0x107E84, 0x2900, true},    {0x197, 0x107E9C, 0x8000, true},   {0xC, 0x107EB0, 0x200000, true},
    {0x19, 0x107EC8, 0x10000, true},    {0x1A, 0x10859C, 0x16D300, false}, {0xF8, 0x107EF8, 0x80000, true},
    {0x18, 0x107F10, 0x1E000, true},    {0xE, 0x107F20, 0x3A000, true},    {0xF1, 0x107F38, 0x1B8000, true},
    {0xF, 0x1085A0, 0x115000, false},   {0x1DD, 0x107F74, 0x5000, true},   {0x16, 0x1085A8, 0x504000, false},
    {0x17, 0x1085AC, 0x1C6D000, false},
};

// `mov r2, #imm`: cond AL, data-processing immediate, MOV, Rd = r2
static uint32_t MovR2(uint32_t encoded) { return 0xE3A02000 | encoded; }

// value as the sum of two ARM immediates (a pair of `sub` instructions), the larger first
static std::pair<uint32_t, uint32_t> TwoImmediates(uint32_t value)
{
    for (int shift = 30; shift >= 0; shift -= 2)
    {
        const uint32_t high = value & (0xFFu << shift);
        if (high && ArmImmediate(high) && ArmImmediate(value - high)) return {*ArmImmediate(high), *ArmImmediate(value - high)};
    }
    throw FormatError("the value is not the sum of two ARM immediates");
}

std::vector<std::string> BuildEngineMod(N3dsRom& oras, const OrasEngineOptions& o, const std::string& outDir)
{
    std::vector<std::string> log;
    std::vector<CodeWord> words;
    const Bytes code = oras.Code();
    char line[160];

    if (o.LinearHeap)
    {
        // the application region must also hold the code and its data as mapped (to 0x6AF000) and the 0xDF0000 normal heap
        const uint32_t total = MemoryModeBytes(o.Memory.value_or(OrasMemoryMode::Prod64)), others = 0x6AF000 - 0x100000 + o.NormalHeap.value_or(0xDF0000);
        if (*o.LinearHeap % 0x1000 || *o.LinearHeap < 0x2B48000 || *o.LinearHeap + others > total)
            throw FormatError("the linear heap must be a multiple of 0x1000, at least the game's 0x2B48000, and fit the memory mode with the code and the normal heap");
        words.push_back({0x106508, 0x02B48000, *o.LinearHeap, "the linear heap FUN_00106448 asks for"});
        snprintf(line, sizeof line, "linear heap 0x%X (%.1f MB; the game's 43.3 MB)", *o.LinearHeap, *o.LinearHeap / 1048576.0);
        log.push_back(line);
    }
    if (o.NormalHeap)
    {
        // the normal heap (`mov r6, #0xDF0000` at 0x10644C) and heap 4, the 0x5ED000 bytes at its top that FUN_00107c0c makes
        // (heaps 8, 0xB, 0xC, 0xD, 0xF8, 0x18 ... are carved from it): every use of 0x5ED000 grows by what the normal heap
        // grows, the rest of the normal heap (FUN_001076fc's region) keeping its size. Heap 4's size is read by FUN_00106448 as
        // two subtractions (0x500000 + 0xED000) and by FUN_00107c0c from the word 0x108564. The same value in the words 0x110254
        // and 0x11AB74 is an address in the game's data (FUN_0011a6c4 reads through it), not this size: patching them too sent
        // the game reading 0xFFD000 in a loop (checked headless, 7 October)
        const auto normal = ArmImmediate(*o.NormalHeap);
        if (!normal || *o.NormalHeap < 0xDF0000) throw FormatError("the normal heap must be at least 0xDF0000 and an ARM immediate (0x1000000, 0x1400000, 0x1800000, 0x2000000 ...)");
        const uint32_t heap4 = 0x5ED000 + (*o.NormalHeap - 0xDF0000);
        const auto [high, low] = TwoImmediates(heap4);
        words.push_back({0x10644C, 0xE3A068DF, 0xE3A06000 | *normal, "the normal heap FUN_00106448 asks for"});
        words.push_back({0x1064C0, 0xE2404605, 0xE2404000 | high, "heap 4's size, subtracted (high part)"});
        words.push_back({0x1064C4, 0xE2444A00 | 0xED, 0xE2444000 | low, "heap 4's size, subtracted (low part)"});
        words.push_back({0x108564, 0x5ED000, heap4, "heap 4's size"});
        char text[128];
        snprintf(text, sizeof text, "normal heap 0x%X (%.1f MB), heap 4 0x%X (%.1f MB; the game's 14.3 and 6.2)", *o.NormalHeap, *o.NormalHeap / 1048576.0, heap4, heap4 / 1048576.0);
        log.push_back(text);
    }
    if (o.ZoneRows)
    {
        if (*o.ZoneRows < 536 || *o.ZoneRows > 1024) throw FormatError("zone rows: 536 to 1024 (the raised bound)");
        words.push_back({0x3D9774, 0xE3550F86, 0xE3550B01, "FUN_003d9740's zone bound"});
        words.push_back({0x3D99F8, 0xE3560F86, 0xE3560B01, "FUN_003d99b8's zone bound"});
        words.push_back({0x112C0C, 0x00007540, *o.ZoneRows * 56, "the zone header table's size"});
        log.push_back("zones: bound 1024, header table " + std::to_string(*o.ZoneRows) + " rows");
    }
    if (o.Characters)
    {
        // FUN_003f7ff4 stops the game when a zone lists more than 26 characters (cmpne r6, #0x1A; zones 0x10, 0x30 and 0x1C3
        // excepted) and before a 27th appears (cmp r0, #0x1A on the count at +0x36AE); each takes an 0xAB0-byte entry of the
        // field manager's pool, made by FUN_00112d4c with 32 entries (mov r3, #0x20 at 0x109048) beside 32 inline 0x1B4-byte
        // records, so 32 is the most the bound may reach without a larger manager
        if (*o.Characters < 26 || *o.Characters > 32) throw FormatError("characters: 26 to 32 (the field manager's pool)");
        words.push_back({0x3F8038, 0x1356001A, 0x13560000 | *o.Characters, "characters a zone may list"});
        words.push_back({0x3F808C, 0xE350001A, 0xE3500000 | *o.Characters, "characters shown at once"});
        log.push_back("characters: " + std::to_string(*o.Characters) + " per zone (the game's 26)");
    }
    for (const auto& [id, size] : o.HeapSizes)
    {
        const HeapSize* h = nullptr;
        for (const HeapSize& k : Heaps) if (k.Id == id) h = &k;
        if (!h) throw FormatError("no heap " + std::to_string(id) + " is made at boot");
        if (size % 0x1000) throw FormatError("a heap size must be a multiple of 0x1000");
        if (h->Immediate)
        {
            const auto encoded = ArmImmediate(size), original = ArmImmediate(h->Size);
            if (!encoded) throw FormatError("heap " + std::to_string(id) + ": its size is an instruction's immediate and cannot hold that value (an 8-bit value rotated by an even amount)");
            words.push_back({h->Address, MovR2(*original), MovR2(*encoded), "heap size"});
        }
        else words.push_back({h->Address, h->Size, size, "heap size"});
        snprintf(line, sizeof line, "heap 0x%X: 0x%X -> 0x%X (%.2f MB)", id, h->Size, size, size / 1048576.0);
        log.push_back(line);
    }

    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = std::filesystem::path(outDir) / "load" / "mods" / id;
    if (o.Memory)
    {
        std::filesystem::create_directories(root);
        WriteFile((root / "exheader.bin").string(), ExHeaderWithSystemMode(oras.ExHeader(), *o.Memory));
        log.push_back("exheader.bin: system mode " + std::to_string((int)*o.Memory));
    }
    if (!words.empty())
    {
        const Bytes ips = CodePatchIps(code, words);
        CodePatchApply(code, ips);
        std::filesystem::create_directories(root / "exefs");
        WriteFile((root / "exefs" / "code.ips").string(), ips);
        log.push_back("code.ips: " + std::to_string(words.size()) + " words");
    }
    return log;
}

}
