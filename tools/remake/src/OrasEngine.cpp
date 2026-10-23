#include "OrasEngine.h"
#include "Bps.h"
#include "CodePatch.h"
#include "OrasSave.h"
#include "OrasXyGymModules.h"
#include "OrasXySound.h"
#include "OrasXyTrainers.h"
#include "OrasXyNatives.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

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

// The area pack's character-model list (a/1/3/7), past 31 models. Two tables hold one entry a model and stop the game when
// full; both are moved to the tail of a grown object, every access rewritten (read 17 October; not yet run in the game).
//
// Table A, the .code's: the field character manager (heap 4, `operator new(0x3C24)` in FUN_00108dd4) caches the list in
// 33 entries of 0x18 bytes at +0x36CC (entry 0 the player's, entry 1 the target of the async load), and
// Event_LoadCharacterArchiveMember stops when entries + 1 reach 33 (cmp #0x21 at 0x3F7300). The entries move to a new tail
// at +0x3C28, 161 of them (the manager 0x4B40 bytes): the list's own entries plus 32, since FUN_003f60fc appends models
// asked for at run time with no bound. Each word: the address, the game's word, the patched one.
struct ModelCacheWord { uint32_t Address, Expected, Value; const char* Why; };
static const ModelCacheWord ModelCacheWords[] = {
    {0x10921C, 0x00003C24, 0x00004B40, "the field character manager's size (FUN_00108dd4)"},
    {0x112F68, 0xE2840B0D, 0xE2840B0F, "its constructor's clear of the cache: base"},
    {0x112F6C, 0xE3A01C03, 0xE3A01C0F, "its constructor's clear of the cache: size"},
    {0x112F70, 0xE2800FB9, 0xE2800040, "its constructor's clear of the cache: entry 1"},
    {0x1E3830, 0xE2833C36, 0xE2833B0F, "FUN_001e37d8's cache base"},
    {0x1E3834, 0xE1D33CBC, 0xE1D332B8, "FUN_001e37d8's model of an entry"},
    {0x3F6128, 0xE2844C36, 0xE2844B0F, "FUN_003f60fc's search base"},
    {0x3F612C, 0xE1D44CBC, 0xE1D442B8, "FUN_003f60fc's model of an entry"},
    {0x3F6160, 0xE2800B0D, 0xE2800B0F, "FUN_003f60fc's appended entry: base"},
    {0x3F6170, 0xE5A032CC, 0xE5A03028, "FUN_003f60fc's appended entry: word 0"},
    {0x3F6308, 0xE2822C36, 0xE2822B0F, "Field_GetCharacterModelInfo's search base"},
    {0x3F630C, 0xE1D22CBC, 0xE1D222B8, "Field_GetCharacterModelInfo's model of an entry"},
    {0x3F6324, 0xE2800B0D, 0xE2800B0F, "Field_GetCharacterModelInfo's entry: base"},
    {0x3F6328, 0xE2801FB3, 0xE2801028, "Field_GetCharacterModelInfo's entry"},
    {0x3F632C, 0xE59042DC, 0xE5904038, "Field_GetCharacterModelInfo's entry + 0x10"},
    {0x3F6330, 0xE59002E0, 0xE590003C, "Field_GetCharacterModelInfo's entry + 0x14"},
    {0x3F6990, 0xE2800B0D, 0xE2800B0F, "Event_ClearAreaAndReleaseHandles's clear: base"},
    {0x3F6994, 0xE3A01C03, 0xE3A01C0F, "Event_ClearAreaAndReleaseHandles's clear: size"},
    {0x3F6998, 0xE2800FB9, 0xE2800040, "Event_ClearAreaAndReleaseHandles's clear: entry 1"},
    {0x3F6D1C, 0xE2833C36, 0xE2833B0F, "Event_RequestCharacterModel's base (odd count)"},
    {0x3F6D20, 0xE1D33CBC, 0xE1D332B8, "Event_RequestCharacterModel's entry i (odd count)"},
    {0x3F6D48, 0xE2811C36, 0xE2811B0F, "Event_RequestCharacterModel's base (pairs)"},
    {0x3F6D4C, 0xE1D13CBC, 0xE1D132B8, "Event_RequestCharacterModel's entry i (pairs)"},
    {0x3F6D54, 0x11D11EB4, 0x11D114B0, "Event_RequestCharacterModel's entry i + 1 (pairs)"},
    {0x3F7258, 0xE2878C36, 0xE2878B0F, "Event_LoadCharacterGraphicsVariants's base"},
    {0x3F725C, 0xE1D85CBC, 0xE1D852B8, "Event_LoadCharacterGraphicsVariants's model of an entry"},
    {0x3F728C, 0xE287BB0D, 0xE287BB0F, "Event_LoadCharacterGraphicsVariants's destination: base"},
    {0x3F7290, 0xE28BBFB3, 0xE28BB028, "Event_LoadCharacterGraphicsVariants's destination"},
    {0x3F72C4, 0xE1C85CBC, 0xE1C852B8, "Event_LoadCharacterGraphicsVariants's model stored"},
    {0x3F732C, 0xE2847B0D, 0xE2847B0F, "Event_LoadCharacterArchiveMember's entry 0: base"},
    {0x3F7338, 0xE2877FB3, 0xE2877028, "Event_LoadCharacterArchiveMember's entry 0"},
    {0x3F74EC, 0xE2883B0D, 0xE2883B0F, "Event_LoadZoneCharacterModels's async destination: base"},
    {0x3F7500, 0xE2833FB9, 0xE2833040, "Event_LoadZoneCharacterModels's async destination: entry 1"},
    {0x3F7574, 0xE2811B0D, 0xE2811B0F, "Event_RegisterCharacterModels's entry: base"},
    {0x3F7578, 0xE2811FB3, 0xE2811028, "Event_RegisterCharacterModels's entry"},
    {0x3F7828, 0x000036CC, 0x00003C28, "Event_RefreshMatchingCharacterModels's cache (literal of ldr r2 at 0x3F7628)"},
    {0x3F7644, 0xE2800C36, 0xE2800B0F, "Event_RefreshMatchingCharacterModels's base"},
    {0x3F7648, 0xE28000DB, 0xE2800037, "Event_RefreshMatchingCharacterModels's entry byte 0xF"},
    {0x4E3BE4, 0xE2800C36, 0xE2800B0F, "Event_AreAllModelsLoaded's base"},
    {0x4E3BE8, 0xE1D01CBC, 0xE1D012B8, "Event_AreAllModelsLoaded's model of an entry"},
};

// Table B, DllField's: the model resource manager ([character manager + 0x36BC], made by Event_CharacterManager_Init with
// operator new(literal 0x3F54CC = 0x2EC8)) holds one slot of 0xA0 bytes a model loaded (word 0 the model cache entry, byte 4
// a second binding, byte 5 which cache, bindings at +8 and +0x54) at +0x40 + 0xA0 i, i < 33, read only by the 10 functions
// patched below (every `i * 0xA0` of DllField and the .code, read 17 October). What follows the slots is not indexed by
// them: 34 entries of 0xB8 at +0x14E0 (resources of fixed kinds, 0x1A and 0x1B named), 20 of 0x10 at +0x2D50 (load
// handles), the object lists at +0x2E90 / +0x2E98 (heap arrays of 0x20 and 2) and fields to +0x2EC8. So the slots move to
// a tail at +0x2F40, the object growing to 0x2F40 + 0xA0 x slots: each `this + 0xA0 i` gains 0x2F00 (in a cave, as the
// pair of instructions computing it has no room for a third), so the offsets the game adds to it (0x40, 0xE0, 0x44 ...)
// stay. The slots: the list's models, the player's and one more, as the game's 33 for 31 (odd: Event_CharacterModelSlotAcquire
// tests slots two at a time). Its .code model cache (Event_ResourceSlots_Init's count at 0x10283928) grows with them.
//
// The caves: the bytes between the end of DllField's code segment (0xF03F0 in the file) and the next page, where the
// rodata segment starts (0xF1000). Zeros, in no segment, the target of no relocation, and executable, since the loader
// makes the code segment's pages executable (azahar/src/core/hle/service/ldr_ro/cro_helper.cpp, GetExecutablePages).
// Azahar only: a console's loader checks the module against the hashes of static.crr (prototype/cro_patch.py).
constexpr uint32_t DllFieldBase = 0x10242000; // where prototype/cro_link.py and the Ghidra program place DllField
constexpr uint32_t SlotsMoved = 0x2F00;       // what `this + 0xA0 i` gains: slot i at this + 0x2F40 + 0xA0 i

struct CroWord { uint32_t Address, Expected, Value; std::string Why; };

static uint32_t Branch(uint32_t from, uint32_t to, bool link)
{
    return (link ? 0xEB000000u : 0xEA000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

// The trainer flag routine (--xy-trainers, BuildEngineMod): r0 a trainer id in, its "beaten" flag out, r1 clobbered
static constexpr uint32_t TrainerFlagRoutine[12] = {
    0xE2401FED, 0xE2511002, // sub r1, r0, #0x3B4; subs r1, r1, #2     r1 = id - 950
    0x2A000002,             // bhs: X/Y's
    0xE2800D1B, 0xE280000C, // add r0, r0, #0x6C0; add r0, r0, #0xC    the game's 0x6CC + id
    0xE12FFF1E,             // bx lr
    0xE3510E31,             // cmp r1, #0x310                          past X/Y's 785
    0x83A00D1B, 0x8280000D, // movhi r0, #0x6C0; addhi r0, r0, #0xD    trainer 1's flag
    0x812FFF1E,             // bxhi lr
    0xE2810C29,             // add r0, r1, #0x2900                     X/Y's 0x2900 + n
    0xE12FFF1E,             // bx lr
};
static_assert(XyTrainerFlagBase == 0x2900 && XyTrainerFirst == 950 && XyTrainerCount == 785 && GameTrainerFlags == 0x6CC, "the routine's immediates");

static uint32_t BranchTarget(uint32_t at, uint32_t word)
{
    const int32_t offset = (int32_t)(word << 8) >> 6; // the 24-bit word offset, sign-extended, times 4
    return at + 8 + offset;
}

// a data-processing instruction with its immediate replaced (cond, opcode and registers kept)
static uint32_t WithImmediate(uint32_t word, uint32_t value)
{
    const auto encoded = ArmImmediate(value);
    if (!encoded || (word & 0x0E000000) != 0x02000000) throw FormatError("not an immediate data-processing instruction, or the value is no ARM immediate");
    return (word & ~0xFFFu) | *encoded;
}

// `add rd, rn, #value` (or sub), always executed
static uint32_t AddImmediate(uint32_t rd, uint32_t rn, uint32_t value, bool subtract = false)
{
    return WithImmediate((subtract ? 0xE2400000u : 0xE2800000u) | rn << 16 | rd << 12, value);
}

static std::vector<CroWord> ModelSlotWords(uint32_t slots, uint32_t cave)
{
    std::vector<CroWord> w;
    const auto bound = [&](uint32_t at, uint32_t expected, const char* why) { w.push_back({at, expected, WithImmediate(expected, slots), why}); };
    // a pair (or triple) of instructions computing this + 0xA0 i: the first becomes a branch to a cave holding them, then
    // the 0x2F00 added to the last one's destination, then a branch back after them
    const auto moved = [&](uint32_t at, std::vector<uint32_t> expected, const char* why) {
        const uint32_t rd = expected.back() >> 12 & 0xF;
        w.push_back({at, expected[0], Branch(at, cave, false), std::string(why) + ": to its cave"});
        for (size_t k = 0; k < expected.size(); k++)
        {
            if (k) w.push_back({at + 4 * (uint32_t)k, expected[k], expected[k], std::string(why) + ": left in place, never reached"});
            w.push_back({cave, 0, expected[k], std::string(why) + " (cave)"});
            cave += 4;
        }
        w.push_back({cave, 0, AddImmediate(rd, rd, SlotsMoved), std::string(why) + " (cave): the slots moved"});
        w.push_back({cave + 4, 0, Branch(cave + 4, at + 4 * (uint32_t)expected.size(), false), std::string(why) + " (cave): back"});
        cave += 8;
    };

    // the constructor (0x10285934): __cxa_vec_ctor on this + 0x40 (33 x 0xA0) returns the array, from which it reaches the
    // 0xB8 entries (+ 0x14A0) and the rest; the array is now at + 0x2F40, so 0x1A60 back
    bound(0x1028593C, 0xE3A03021, "the constructor's slot count");
    w.push_back({0x1028594C, 0xEBFEF2D3, Branch(0x1028594C, cave, true), "the constructor's __cxa_vec_ctor call: to its cave"});
    w.push_back({cave, 0, AddImmediate(0, 0, SlotsMoved), "the constructor (cave): the slots moved"});
    w.push_back({cave + 4, 0, Branch(cave + 4, BranchTarget(0x1028594C, 0xEBFEF2D3), false), "the constructor (cave): __cxa_vec_ctor"});
    cave += 8;
    w.push_back({0x10285950, 0xE2800A01, AddImmediate(0, 0, 0x1A00, true), "the constructor: from the slots to the 0xB8 entries (1/2)"});
    w.push_back({0x10285960, 0xE2800E4A, AddImmediate(0, 0, 0x60, true), "the constructor: from the slots to the 0xB8 entries (2/2)"});
    // the destructors' __cxa_vec_dtor (Field_ManagerDestroy, FUN_10285a98)
    bound(0x10285A70, 0xE3A03021, "Field_ManagerDestroy's slot count");
    w.push_back({0x10285A78, 0xE2840040, AddImmediate(0, 4, SlotsMoved + 0x40), "Field_ManagerDestroy's slots"});
    bound(0x10285AE0, 0xE3A03021, "FUN_10285a98's slot count");
    w.push_back({0x10285AE8, 0xE2840040, AddImmediate(0, 4, SlotsMoved + 0x40), "FUN_10285a98's slots"});
    // Event_CharacterModelSlotAcquire: slot 0, then slots i and i + 1 for odd i
    w.push_back({0x10284084, 0xE5940040, Branch(0x10284084, cave, false), "Event_CharacterModelSlotAcquire's slot 0: to its cave"});
    w.push_back({cave, 0, AddImmediate(0, 4, SlotsMoved), "Event_CharacterModelSlotAcquire's slot 0 (cave)"});
    w.push_back({cave + 4, 0, 0xE5900040, "Event_CharacterModelSlotAcquire's slot 0 (cave): ldr r0, [r0, #0x40]"});
    w.push_back({cave + 8, 0, Branch(cave + 8, 0x10284088, false), "Event_CharacterModelSlotAcquire's slot 0 (cave): back"});
    cave += 12;
    w.push_back({0x10284090, 0x02845040, WithImmediate(0x02845040, SlotsMoved + 0x40), "Event_CharacterModelSlotAcquire's slot 0 taken"});
    moved(0x1028409C, {0xE0802100, 0xE0841282}, "Event_CharacterModelSlotAcquire's search");
    bound(0x102840C0, 0xE3500021, "Event_CharacterModelSlotAcquire's bound");
    w.push_back({0x102840D8, 0xE2845040, WithImmediate(0xE2845040, SlotsMoved + 0x40), "Event_CharacterModelSlotAcquire's slot 0 when all are taken"});
    moved(0x1028411C, {0xE0801100, 0xE0840281}, "Event_CharacterModelSlotAcquire's slot i taken");
    moved(0x1028412C, {0xE0801100, 0xE0840281}, "Event_CharacterModelSlotAcquire's slot i + 1 taken");
    // Event_ReleaseCharacterGraphics, and two functions Ghidra left unnamed (0x10284ACC: a model's slot released;
    // 0x10284B70: a model's slot passed on to 0x102C27FC)
    moved(0x10284848, {0xE0841104, 0xE0860281}, "Event_ReleaseCharacterGraphics's slot i");
    bound(0x1028488C, 0xE3540021, "Event_ReleaseCharacterGraphics's bound");
    moved(0x10284AD4, {0xE0823102, 0xE0803283}, "0x10284ACC's search");
    moved(0x10284AE8, {0xE0821102, 0xE0800281}, "0x10284ACC's slot found");
    bound(0x10284B2C, 0xE3520021, "0x10284ACC's bound");
    moved(0x10284B74, {0xE0823102, 0xE0803283}, "0x10284B70's search");
    moved(0x10284B88, {0xE0821102, 0xE0801281}, "0x10284B70's slot found");
    bound(0x10284B9C, 0xE3520021, "0x10284B70's bound");
    // Field_SubsystemReset: the clear loop (count down in r1; its `subs` sets the flags the loop's bne reads, and is carried
    // into the cave unchanged, the add after it setting none)
    bound(0x102C24F8, 0xE3A01021, "Field_SubsystemReset's slot count");
    moved(0x102C2500, {0xE0803100, 0xE2511001, 0xE0852283}, "Field_SubsystemReset's slot i");
    // Event_FindActiveEntryById, and the slot state query Ghidra calls Snd_IsTrackPlaying
    moved(0x102C2F9C, {0xE0823102, 0xE0803283}, "Event_FindActiveEntryById's search");
    moved(0x102C2FD0, {0xE0821102, 0xE0800281}, "Event_FindActiveEntryById's slot found");
    bound(0x102C2FE4, 0xE3520021, "Event_FindActiveEntryById's bound");
    moved(0x102C30B4, {0xE0823102, 0xE0803283}, "Snd_IsTrackPlaying's search");
    moved(0x102C30E8, {0xE0821102, 0xE0800281}, "Snd_IsTrackPlaying's slot found");
    bound(0x102C3110, 0xE3520021, "Snd_IsTrackPlaying's bound");
    // the .code model cache Field_SubsystemInit makes for the slots (Event_ResourceSlots_Init, 0x28 bytes an entry, sized
    // by this count; Event_CharacterModelCache_Acquire stops the game when it is full)
    bound(0x10283928, 0xE3A03021, "Field_SubsystemInit's model cache count");
    return w;
}

// the module with those words written, each checked: the game's word where one is expected, never a word the loader
// relocates (it would be overwritten), a cave word inside the executable pages past the code segment and zero before
// a CRO's layout as Azahar's loader reads it (azahar/src/core/hle/service/ldr_ro/cro_helper.h): a 0x80-byte hash, then u32
// header fields (18 and 19 the segment table and its count, 30/31, 42/43 and 44/45 the external, internal and static
// relocation tables), a segment entry {file offset, size, type}, a relocation entry 12 bytes from its target's tag (segment
// index in the low 4 bits, offset in the rest)
struct CroLayout
{
    const Bytes& Data;
    explicit CroLayout(const Bytes& data) : Data(data)
    {
        if (U32(0x80) != 0x304F5243) throw FormatError("not a CRO module (no CRO0)");
        for (uint32_t i = 0; i < Field(19); i++) Segments.push_back({U32(Field(18) + 12 * i), U32(Field(18) + 12 * i + 4)});
        if (Segments.empty() || U32(Field(18) + 8) != 0) throw FormatError("the module's first segment is not its code");
    }
    uint32_t U32(size_t at) const
    {
        if (at + 4 > Data.size()) throw FormatError("the module is cut short");
        return (uint32_t)(Data[at] | Data[at + 1] << 8 | Data[at + 2] << 16 | (uint32_t)Data[at + 3] << 24);
    }
    uint32_t Field(int index) const { return U32(0x80 + 4 * (size_t)index); }
    uint32_t CodeEnd() const { return Segments[0].first + Segments[0].second; }
    std::vector<std::pair<uint32_t, uint32_t>> Segments; // file offset, size
};

static Bytes PatchCro(const Bytes& cro, uint32_t base, const std::vector<CroWord>& words)
{
    const CroLayout layout(cro);
    const auto u32 = [&](size_t at) { return layout.U32(at); };
    const auto field = [&](int index) { return layout.Field(index); };
    const auto& segments = layout.Segments;
    const uint32_t codeEnd = layout.CodeEnd(), pagesEnd = (codeEnd + 0xFFF) & ~0xFFFu;
    std::map<uint32_t, std::string> written; // as CodePatchIps: two patches on one word refused
    for (const CroWord& w : words)
        if (!written.emplace(w.Address, w.Why).second) throw FormatError("module patch: " + w.Why + " and " + written[w.Address] + " both write the same word");
    for (const auto& [offset, size] : segments)
        if (size && offset < pagesEnd && offset + size > codeEnd) throw FormatError("a segment lies in the code's last page");
    std::set<uint32_t> relocated;
    for (const auto& [table, count] : {std::pair{field(30), field(31)}, std::pair{field(42), field(43)}, std::pair{field(44), field(45)}})
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t tag = u32(table + 12 * i);
            if ((tag & 0xF) < segments.size()) relocated.insert(segments[tag & 0xF].first + (tag >> 4));
        }
    Bytes out = cro;
    for (const CroWord& w : words)
    {
        const uint32_t at = w.Address - base;
        char where[96];
        snprintf(where, sizeof where, "module 0x%08X (file 0x%X): ", w.Address, at);
        if (at < segments[0].first || at + 4 > pagesEnd) throw FormatError(std::string(where) + "outside the code's pages (" + w.Why + ")");
        if (at >= codeEnd && w.Expected != 0) throw FormatError(std::string(where) + "a cave word must be expected zero (" + w.Why + ")");
        if (relocated.count(at)) throw FormatError(std::string(where) + "the loader relocates this word (" + w.Why + ")");
        if (u32(at) != w.Expected)
        {
            char text[64];
            snprintf(text, sizeof text, "holds 0x%08X, not 0x%08X (", u32(at), w.Expected);
            throw FormatError(std::string(where) + text + w.Why + "): another version of the game?");
        }
        for (int k = 0; k < 4; k++) out[at + k] = (uint8_t)(w.Value >> (8 * k));
    }
    return out;
}

// a literal word the loader relocates (an internal relocation, to `expected` in the module) made to hold `value`, an address
// outside the module (the room): each relocation entry of the word made a copy of an entry relocating another word, which
// the loader then writes twice with the same value (R_ARM_ABS32 and the other kinds write, they do not add: Azahar's
// CROHelper::ApplyRelocation; unloading writes 0 there twice), and the word written with the value. Every entry of the word
// is replaced: a word made a copy of before (two literals side by side, DllBackGround's backdrops then grounds) has two,
// and the one left kept the game's table for the grounds (X/Y's grounds from 93 read the table after, one ground for all:
// runs bgv1-bgv62, 22 October)
static void PointLiteralAt(Bytes& cro, uint32_t base, uint32_t literal, uint32_t expected, uint32_t value, const std::string& why)
{
    const CroLayout layout(cro);
    const uint32_t relocations = layout.Field(42), count = layout.Field(43), at = literal - base - layout.Segments[0].first;
    const auto target = [&](uint32_t i) { const uint32_t tag = layout.U32(relocations + 12 * i); return (tag & 0xF) == 0 ? tag >> 4 : 0xFFFFFFFFu; };
    uint32_t other = count;
    for (uint32_t i = 0; i < count && other == count; i++) if (target(i) != at && (layout.U32(relocations + 12 * i) & 0xF) == 0) other = i;
    if (other == count) throw FormatError(why + ": no other relocation to copy");
    bool found = false;
    for (uint32_t i = 0; i < count; i++)
    {
        if (target(i) != at) continue;
        const uint32_t entry = relocations + 12 * i;
        const uint8_t segment = cro[entry + 5];
        if (segment >= layout.Segments.size() || base + layout.Segments[segment].first + layout.U32(entry + 8) != expected)
            throw FormatError(why + ": its relocation is not the one read (another version of the game?)");
        std::copy(cro.begin() + relocations + 12 * other, cro.begin() + relocations + 12 * other + 12, cro.begin() + entry);
        found = true;
    }
    if (!found) throw FormatError(why + ": no relocation of its literal word");
    for (int k = 0; k < 4; k++) cro[literal - base + k] = (uint8_t)(value >> (8 * k));
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

        // a zone script's native mask comes from a 536-word table (0x587D58, one word a zone) that Zone_LoadZoneScript reads
        // with `ldr r0, [r0, r4, lsl #2]` (r4 the zone) at 0x3FF5AC; zones from 536 read the words after it (ORAS_ENGINE.md
        // 6). That load becomes a call to a routine in the zeros closing the code segment (the exheader's text ends at
        // 0x579604, its pages at 0x57A000: loaded and executable, unused): below 536 the old load, else a word of a table of
        // 488 masks for zones 536-1023, the common 0x243 unless given. Only r0 and the flags change; lr is the caller's own
        // (Zone_LoadZoneScript calls functions after it, so it has saved its lr)
        const uint32_t cave = 0x579608, table = cave + 28;
        words.push_back({0x3FF5AC, 0xE7900104, 0xEB000000 | (((cave - 0x3FF5AC - 8) >> 2) & 0xFFFFFF), "Zone_LoadZoneScript's mask load -> bl the mask routine"});
        const uint32_t routine[] = {
            0xE3540F86,  // cmp r4, #0x218
            0x37900104,  // ldrlo r0, [r0, r4, lsl #2]   the game's table
            0x312FFF1E,  // bxlo lr
            0xE2440F86,  // sub r0, r4, #0x218
            0xE08F0100,  // add r0, pc, r0, lsl #2       (pc = here + 8)
            0xE5900004,  // ldr r0, [r0, #4]             the word at table + 4 x (zone - 536)
            0xE12FFF1E,  // bx lr
        };
        for (uint32_t k = 0; k < 7; k++) words.push_back({cave + 4 * k, 0, routine[k], "the mask routine"});
        for (const auto& [zone, mask] : o.ScriptMasks)
            if (zone < 536 || zone > 1023) throw FormatError("script mask: zones 536 to 1023 (the game's own table holds 0-535)");
        for (uint32_t zone = 536; zone < 1024; zone++)
        {
            const auto given = o.ScriptMasks.find(zone);
            words.push_back({table + 4 * (zone - 536), 0, given != o.ScriptMasks.end() ? given->second : 0x243, "a zone's script mask"});
        }
        log.push_back("script masks: zones 536-1023 from a new table (0x243 but " + std::to_string(o.ScriptMasks.size()) + " given)");
    }
    else if (!o.ScriptMasks.empty()) throw FormatError("--script-mask needs --zone-rows (zones past 536)");
    if (o.Characters)
    {
        // FUN_003f7ff4 stops the game when a zone lists more than 26 characters (cmpne r6, #0x1A; zones 0x10, 0x30 and 0x1C3
        // excepted) and before a 27th appears (cmp r0, #0x1A on the count at +0x36AE); each takes an 0xAB0-byte entry of the
        // field manager's pool, allocated apart from the manager by FUN_00112d4c with the count its caller passes (mov r3,
        // #0x20 at 0x109048): above 32 the pool grows with the bound. What then bounds a zone is its events file, copied on
        // entry into a 0xC84-byte buffer of the save data (FUN_003db6e4; OrasZone::Write refuses a larger one): 35 clones
        // in Littleroot fit and show, 36 stop the game there (checked headless, ORAS_ENGINE.md 4.5)
        if (*o.Characters < 26 || *o.Characters > 255) throw FormatError("characters: 26 to 255");
        words.push_back({0x3F8038, 0x1356001A, 0x13560000 | *o.Characters, "characters a zone may list"});
        words.push_back({0x3F808C, 0xE350001A, 0xE3500000 | *o.Characters, "characters shown at once"});
        if (*o.Characters > 32) words.push_back({0x109048, 0xE3A03020, 0xE3A03000 | *o.Characters, "the field manager's character pool"});
        log.push_back("characters: " + std::to_string(*o.Characters) + " per zone (the game's 26)" + (*o.Characters > 32 ? ", pool " + std::to_string(*o.Characters) : ""));
    }
    if (o.Characters && *o.Characters > 31)
    {
        // Saving writes one MoveModelSave record (save block 10, 0x108 bytes) per live entry of the character pool
        // (FUN_004e3c64 -> FUN_0045c4c4) and Continue restores them (FUN_001e7698 -> FUN_0045c1e0 -> FUN_0045c2d4), both
        // bounded by 32 records inline in the SaveData object (operator new(0x723B8) in FUN_00108dd4, MoveModelSave's vtable
        // 0x5E064C at +0x1314, its records at +0x1318, Misc's object right after them at +0x3418), so the block cannot grow in
        // place. The records move to a new tail of the SaveData, at +0x73314 (+0x1314 + 0x72000, one ARM immediate), N + 1
        // of them; the 32 inline ones are left unused. The file follows by itself: each block's offset is the sum of the
        // aligned sizes before it, and the file's size their sum, both taken from the size getters (Save_ReadBlocks,
        // Save_WriteBlocks, FUN_00115a84 at boot; no size constant in the code), so the blocks after 10 move and the file
        // grows (OrasSave::ResizeMoveModels writes that layout). A save of the old layout is then too short: Save_ReadBlocks
        // returns 4 on its short read (owner's decision, 17 October: old saves do not matter). Read 17 October, not yet run
        const uint32_t records = *o.Characters + 1, bytes = (uint32_t)OrasMoveModelBytes(records);
        const uint32_t tail = 0x72000, size = 0x73314 + bytes; // the records at SaveData + 0x1314 + tail
        const auto imm = [](uint32_t v) { const auto e = ArmImmediate(v); if (!e) throw FormatError("not an ARM immediate"); return *e; };
        const uint32_t count = imm(records);
        words.push_back({0x1091F0, 0x000723B8, size, "the SaveData's size (FUN_00108dd4's operator new), the records' tail after it"});
        words.push_back({0x45C6A8, 0xE2800004, 0xE2800000 | imm(tail), "MoveModelSave's data getter (vtable +0xC): the tail"});
        words.push_back({0x45C2CC, 0xE3A00C21, 0xE3A00000 | imm(bytes), "MoveModelSave's size getter (vtable +0x10): the block's size"});
        words.push_back({0x45C6B0, 0xE3A02C21, 0xE3A02000 | imm(bytes), "MoveModelSave's clear (vtable +0x8): its size"});
        words.push_back({0x45C6B4, 0xE2800004, 0xE2800000 | imm(tail), "MoveModelSave's clear (vtable +0x8): the tail"});
        words.push_back({0x45C690, 0xE2800004, 0xE2800000 | imm(tail), "FUN_0045c678's clear loop (vtable +0x14): the tail"});
        words.push_back({0x45C69C, 0xE3540020, 0xE3540000 | count, "FUN_0045c678's clear loop: the records"});
        // the save and Continue pass SaveData + 0x1314 to the record functions, which add 4 + 0x108 x index: they pass the tail - 4
        words.push_back({0x4E3C74, 0xE2808A01, 0xE2808000 | imm(0x73000), "FUN_004e3c64 (save): the records' base, high part"});
        words.push_back({0x4E3C78, 0xE2888FC5, 0xE2888FC4, "FUN_004e3c64 (save): the records' base, + 0x310"});
        words.push_back({0x45C200, 0xE280AA01, 0xE280A000 | imm(0x73000), "FUN_0045c1e0 (Continue): the records' base, high part"});
        words.push_back({0x45C208, 0xE28AAFC5, 0xE28AAFC4, "FUN_0045c1e0 (Continue): the records' base, + 0x310"});
        words.push_back({0x45C2C0, 0xE3540020, 0xE3540000 | count, "FUN_0045c29c's clear loop (save, and Continue's end): the records"});
        words.push_back({0x4E8804, 0xE3510020, 0xE3510000 | count, "FUN_004e8800 (a record in use): the records"});
        words.push_back({0x45C4C8, 0xE3510020, 0xE3510000 | count, "FUN_0045c4c4 (a record written): the records"});
        words.push_back({0x45C2D8, 0xE3510020, 0xE3510000 | count, "FUN_0045c2d4 (a record restored): the records"});
        snprintf(line, sizeof line, "save: block 10 holds %u characters (the game's 32), 0x%X bytes; the SaveData 0x%X bytes; saves need oras-save --save-records %u", records, bytes, size, records);
        log.push_back(line);
    }
    // Each shown character's model is a field object (Event_SpawnCharacterModel -> Event_AllocFieldObject, 0x2F0 bytes,
    // allocated on use from the manager's heap 0x62 at +0x10) listed in a heap array at +0x2E90, whose length
    // Event_CharacterManager_Init passes as 32 (`mov r1, #0x20` at 0x3F525C, stored at +0x2E94). Full, the game does not
    // stop (its check, FUN_0014ea64, is an empty function): it hands back the last object, so the characters past it share
    // one object (read 17 October). Growing the length alone hung the game on entering the field (run hK, headless): heap
    // 0x62 is 0x10000 bytes (`mov r2, #0x10000` at DllField 0x10283954, Field_SubsystemInit), sized for 32 objects, so it
    // grows in proportion (taken from the manager's parent heap; that this was the hang is inferred, not yet run)
    std::optional<uint32_t> fieldObjects, fieldObjectHeap;
    if (o.Characters && *o.Characters + 6 > 32)
    {
        uint32_t objects = *o.Characters + 6, heap = 0;
        while (!ArmImmediate(objects)) objects++;
        heap = (0x10000 * objects / 32 + 0xFFF) & ~0xFFFu;
        while (!ArmImmediate(heap)) heap += 0x1000;
        fieldObjects = objects;
        fieldObjectHeap = heap;
        words.push_back({0x3F525C, 0xE3A01020, 0xE3A01000 | *ArmImmediate(objects), "the field objects of the character models (Event_CharacterManager_Init)"});
        snprintf(line, sizeof line, "character model objects: %u (the game's 32), their heap 0x62 0x%X bytes (the game's 0x10000)", objects, heap);
        log.push_back(line);
    }
    if (o.LargePacks)
    {
        // Zone_ChangeLoadStep (0x3D5698) passes the zone's area pack (Zone_HeaderGetAreaId, a u16) cut to a byte to the loader
        // of the zone's character models (Event_LoadZoneCharacterModels, its a/1/3/7 member) and to Event_LoadAnimSetIfChanged
        // (its 0x2C-byte entry of archive 0xE4, at pack x 0x2C), so pack 258 loaded pack 2's: the house of X/Y's zone 2 on pack
        // 258 stopped the game in Event_CharacterPool_Acquire, on 255 it entered (runs h_wK37, h_wK34, 19 October). The three
        // `and r1, r0, #0xff` pass it whole: both read it as a u16 (Res_ArchiveMemberSize, a short index); the second keeps a
        // byte of it as its cache, which then never matches a pack past 255 and reloads its entry, as on a change of pack
        for (const uint32_t at : {0x3D5728u, 0x3D5960u, 0x3D59A4u})
            words.push_back({at, 0xE20010FF, 0xE1A01000, "Zone_ChangeLoadStep: the area pack whole (mov r1, r0)"});
        // Event_LoadAnimSetIfChanged keeps the pack as a byte at +0x30 of the area object (EventWork's field context + 0x2C;
        // its archive-14 handle at +0x34), and the area loader takes the pack's a/0/1/4 member from it (0x40483C, `ldrb sb,
        // [r5, #0x30]`, then its size and Res_AsyncLoadMember): pack 258 loaded member 2, the house then ran out of memory
        // loading its piece (run h_wK37b; found by the memory dumps hMd34/hMd37 and the code around the area handle). Kept
        // as a u16 at +0x30 and read so by its three readers; the byte at +0x31 (the variant the cache compares) is no longer
        // written, so with a variant other than 0 the 44-byte entry is reloaded on a zone change, as on a change of pack
        words.push_back({0x402CF4, 0xE5C45030, 0xE1C453B0, "Event_LoadAnimSetIfChanged: the pack kept whole (strh r5, [r4, #0x30])"});
        words.push_back({0x402CFC, 0xE5C46031, 0xE320F000, "Event_LoadAnimSetIfChanged: the variant byte, now the pack's high byte, not written"});
        words.push_back({0x402C90, 0xE5D40030, 0xE1D403B0, "Event_LoadAnimSetIfChanged: its cache compares the pack whole (ldrh)"});
        words.push_back({0x3D592C, 0xE5D71030, 0xE1D713B0, "Zone_ChangeLoadStep: a change of pack seen whole (ldrh)"});
        words.push_back({0x40483C, 0xE5D59030, 0xE1D593B0, "the area loader: the pack's a/0/1/4 member whole (ldrh sb, [r5, #0x30])"});
        log.push_back("area packs: past 255 (the zone change passes the pack whole)");
    }
    if (o.LargeStory)
    {
        // Story flags and event works past the game's (ORAS_ENGINE.md 6, Story flags; read 19 October). Flags 1-0xCFF are
        // bits of the EventWork object (save block 19, SaveData + 0x13140) at +0x2F4, read by FUN_0012fd48, set by
        // Save_SetEventFlag (0x406B30), cleared by Save_ClearEventFlag_0011FA10 (0x11F9E8 leads to one or the other); each
        // sends 0xD00-0x2FFF to the error routine 0x14EA64 (get: answers 0). Works 0x4000-0x4177 are u16 at +4, their address
        // from FUN_0011fa90, whose range check the build left empty (a work past 0x4177 lands on the flags and beyond). Save
        // block 47, ReservedSaveData (2000 bytes), has only the methods every block has (size 0x7D0, data at +4, copy, clear;
        // its vtable 0x5E074C is named once, by Save_SaveData_Construct) and is zero in the owner's two saves: unused,
        // inferred. Its object lies 0xA54C after EventWork's (measured in memory, run hSave2), so its data at EventWork +
        // 0xA550: works 0x4178-0x436B its first 1000 bytes, flags 0xD00-0x2C3F its last 1000. Saved and loaded with the
        // block, the save's layout unchanged. Code that reaches the bits without these four functions is not covered (none
        // found; not searched beyond their callers)
        const uint32_t cave = 0x579E00; // past the script-mask routine and its table (0x579608-0x579DC3), zeros to 0x57A000
        uint32_t at = cave;
        const auto bcs = [](uint32_t from, uint32_t to) { return 0x2A000000u | (((to - from - 8) >> 2) & 0xFFFFFF); };
        struct FlagSite { uint32_t Branch, Fail, Use; const char* Name; };
        for (const FlagSite& f : {FlagSite{0x12FD68, 0x12FD7C, 0x12FDAC, "the flag reader FUN_0012fd48"},
                                  FlagSite{0x11FA2C, 0x11FA40, 0x11FA64, "the flag clearer 0x11FA10"},
                                  FlagSite{0x406B4C, 0x406B60, 0x406B84, "the flag setter Save_SetEventFlag"}})
        {
            words.push_back({f.Branch, bcs(f.Branch, f.Fail), bcs(f.Branch, at), std::string(f.Name) + ": flags 0xD00 and up to its cave"});
            const uint32_t routine[] = {
                0xE2412C0D,          // sub r2, r1, #0xD00
                0xE3520D7D,          // cmp r2, #0x1F40           8000 flags
                bcs(at + 8, f.Fail), // bhs the game's error path
                0xE2800CA9,          // add r0, r0, #0xA900       r0 is EventWork here
                0xE2800038,          // add r0, r0, #0x38         block 47's data + 1000
                0xE08001A2,          // add r0, r0, r2, lsr #3
                Branch(at + 24, f.Use, false),
            };
            for (uint32_t k = 0; k < 7; k++) words.push_back({at + 4 * k, 0, routine[k], std::string(f.Name) + " (cave)"});
            at += 28;
        }
        words.push_back({0x11FAA8, 0xE3A02000, Branch(0x11FAA8, at, false), "FUN_0011fa90: works past 0x4177 to its cave"});
        const uint32_t works[] = {
            0xE2440C41,          // sub r0, r4, #0x4100       r4 the work, r5 EventWork
            0xE2400078,          // sub r0, r0, #0x78
            0xE3500F7D,          // cmp r0, #0x1F4            500 works
            bcs(at + 12, 0x11FAB8), // bhs the game's own address
            0xE0850080,          // add r0, r5, r0, lsl #1
            0xE2800CA5,          // add r0, r0, #0xA500
            0xE2800050,          // add r0, r0, #0x50         block 47's data
            0xE8BD8070,          // ldmia sp!, {r4, r5, r6, pc}
        };
        for (uint32_t k = 0; k < 8; k++) words.push_back({at + 4 * k, 0, works[k], "FUN_0011fa90 (cave)"});
        log.push_back("story: flags 0xD00-0x2C3F and works 0x4178-0x436B in save block 47 (the game's flags end at 0xCFF, works at 0x4177)");
    }
    if (o.Badges16)
    {
        // The badges (read 21 October): a u32 at the player's status (game data + 0x3418) + 0x14, one bit a badge. Read by
        // FUN_004E9E1C (BadgeGetFlag's), set by 0x462628, cleared by 0x46273C, each refusing a number from 8 (`cmp r1, #8`, to the
        // fatal-error call); counted by FUN_004E9E64 (BadgeGetCount's and 7 callers': the level cap through FUN_003CB784,
        // the mart's stock under 5, the trainer card, the battle's settings) and inline by FUN_004E9ED4 (the money lost, a
        // table of 9 indexed by the count). No other code reads the word (searched: every "+ 0x3418 + 0x14" in the .code and
        // the modules, and the status functions' +0x14 accesses). Numbers 8-15 allowed; both counts become the badges of
        // either region: popcount((bits | bits >> 8) & 0xFF), each region's own count when only one has badges, at most 8
        // (the tables' bound) when both do
        for (const uint32_t at : {0x4E9E1Cu, 0x462628u, 0x46273Cu})
            words.push_back({at, 0xE3510008, 0xE3510010, "a badge number bound 8 -> 16 (cmp r1, #16)"});
        const std::vector<std::pair<uint32_t, uint32_t>> count = {
            {0xE5900014, 0xE5900014}, {0xE3A01000, 0xE1800420}, {0xE3500000, 0xE20000FF}, {0x0A000003, 0xE3A01000},
            {0xE3100001, 0xE1B000A0}, {0x12811001, 0xE2A11000}, {0xE1B000A0, 0x1AFFFFFC}, {0x1AFFFFFB, 0xE1A00001},
            {0xE20100FF, 0xE12FFF1E}, {0xE12FFF1E, 0xE12FFF1E}};
        for (size_t k = 0; k < count.size(); k++)
            words.push_back({0x4E9E64 + 4 * (uint32_t)k, count[k].first, count[k].second, "the badge count, either region's (ldr; orr lsr 8; and 0xFF; lsrs/adc loop)"});
        const std::vector<std::pair<uint32_t, uint32_t>> money = {
            {0xE5902014, 0xE5902014}, {0xE3A03000, 0xE1822422}, {0xE3520000, 0xE20220FF}, {0x0A000003, 0xE3A03000},
            {0xE3120001, 0xE1B020A2}, {0x12833001, 0xE2A33000}, {0xE1B020A2, 0x1AFFFFFC}, {0x1AFFFFFB, 0xE1A02003},
            {0xE20320FF, 0xE1A00000}};
        for (size_t k = 0; k < money.size(); k++)
            words.push_back({0x4E9ED4 + 4 * (uint32_t)k, money[k].first, money[k].second, "the money lost's badge count, the same"});
        log.push_back("badges: 16 (Hoenn's 0-7, Kalos's 8-15), counted by region");
    }
    if (o.XyTrainers)
    {
        // X/Y's trainers after the game's 950 (OrasXyTrainers.h). The data readers bound a trainer id by literal words 950
        // (seven words read by ten readers: Trainer_ReadData 0x453E08, Trainer_BuildOneTeam 0x454EA4, Trainer_FillInfo
        // 0x455450 and seven others; id 0 or past the bound reads trainer 1) and 949 (0x443AC0, a battle setup's check of
        // its trainer), none of the eight loaded by another instruction (checked 20 October); the trainer a character's script names (3000 + id, 5000 + id for a double's second)
        // is bounded by 980 in the getter FUN_004dc648 (DllField's import 0x13C0), Event_IsWarpEnabledByFlag and DllField
        // 0x21328, the room of the "beaten" flags 0x6CC + id. The bound becomes 1792 (0x700, an immediate) and the flag
        // comes from one routine: 0x6CC + id below 950 as the game's, 0x2900 + (id - 950) for X/Y's, past them trainer 1's
        // as the game does past its bound. In the .code at 0x579F80-0x579FAF and a copy in DllField's cave (DllField cannot
        // branch to the .code: it is loaded where the game puts it); r0 the id in and the flag out, r1 clobbered (each
        // site's next instruction writes r1, and lr is saved by each of the four functions)
        if (!o.LargeStory) throw FormatError("--xy-trainers needs --story large (X/Y's trainer flags are 0x2900 and up)");
        const uint32_t count = XyTrainerCount, last = XyTrainerFirst + XyTrainerCount;
        for (const uint32_t pool : {0x450FBCu, 0x4533ACu, 0x453504u, 0x453EA4u, 0x454D90u, 0x45544Cu, 0x455600u})
            words.push_back({pool, 0x3B6, last, "a trainer data reader's bound (950 -> " + std::to_string(last) + ")"});
        words.push_back({0x443EC8, 0x3B5, last - 1, "a battle setup's last trainer (949)"});
        constexpr uint32_t cave = 0x579F80;
        for (uint32_t k = 0; k < 12; k++) words.push_back({cave + 4 * k, 0, TrainerFlagRoutine[k], "trainer flags (cave)"});
        words.push_back({0x4DC66C, 0xE3500FF5, 0xE3500C07, "the trainer getter's bound (980 -> 1792)"});
        words.push_back({0x4DE5D4, 0xE3550FF5, 0xE3550C07, "Event_IsWarpEnabledByFlag's trainer bound (980 -> 1792)"});
        words.push_back({0x4DE5F0, 0xE2850B01, 0xE1A00005, "Event_IsWarpEnabledByFlag: the id to r0"});
        words.push_back({0x4DE5F4, 0xE2800FB3, Branch(0x4DE5F4, cave, true), "Event_IsWarpEnabledByFlag: its flag from the routine"});
        log.push_back("trainers: " + std::to_string(last) + " (X/Y's " + std::to_string(count) + " after the game's 950), their flags 0x2900 and up");
        // X/Y's trainer classes after the game's 280 (OrasXyTrainers.h: 178, as 280-457). The classes are read at boot into a
        // table of 0x14 bytes each after a 0x10-byte head (0x108E58 allocates 0x15F0 for 280, 0x108E8C hands 280 to the
        // loader, whose loop 0x10EA9C reads a/0/3/7 member by member up to 280 through 0x128C7C, itself bounded); each
        // reader bounds the class by 280 and takes the table's first entry past it (Trainer_LoadInfoAndName 0x1F019C,
        // Trainer_ReadClass 0x4540CC, Trainer_BuildTeams 0x454434, Trainer_BuildOneTeam 0x454EA4; read 22 October; the other
        // compares with 280 in the .code and the modules are not classes: move switches, sizes). 460 (0x1CC, an immediate;
        // 458 is not), two left free. Trainer_Class_GetIndexedEntry's own bound (drawing 0x2AB) below
        for (const auto& [at, from] : std::vector<std::pair<uint32_t, uint32_t>>{{0x10EAB4, 0xE3540F46}, {0x128C80, 0xE3520F46}, {0x1F01BC, 0xE3560F46},
                                                                                {0x4540D0, 0xE3520F46}, {0x4545C0, 0xE3570F46}, {0x455034, 0xE35B0F46}})
            words.push_back({at, from, (from & ~0xFFFu) | 0xF73u, "a trainer class bound (280 -> 460)"});
        words.push_back({0x108E8C, 0xE3A03F46, 0xE3A03F73, "the trainer classes loaded at boot (280 -> 460)"});
        words.push_back({0x109200, 0x15F0, 0x10 + 460 * 0x14, "the class table's size (0x15F0 -> 0x2400)"});
        // the drawings a class names (a/1/6/0, OrasXyTrainers.cpp): Trainer_Class_GetIndexedEntry reads past 0x2AB as the
        // variant alone (Omega Ruby's 228 groups end there); X/Y's appended groups past it, up to 1024 drawings
        words.push_back({0x4E79B8, 0xE3500FAB, 0xE3500B01, "Trainer_Class_GetIndexedEntry's bound (drawing 0x2AC -> 0x400)"});
        log.push_back("trainer classes: 460 (X/Y's after the game's 280)");
    }
    if (o.LargeEvents)
    {
        // A zone's events file is copied on entering it into one of two 0xC8C-byte slots of the events object (0x1958 bytes,
        // FUN_00108dd4), slot 0 at +0x34 (+0x194C points to it), its size at +0xC84 and its zone at +0xC88; past 0xC84
        // bytes the game stops (Event_LoadZoneEventsFile 0x3DB6E4). Slot 1 (+0xCC0, +0x1950) has no user found (every
        // caller passes slot 0). Not saved: Continue rereads the zone's file (EventGameStart::EventFirst ->
        // Zone_ChangeLoadStep), characters' positions come from save block 10 (read 17 October). So slot 0 takes both
        // slots' room, the size and zone moved in front of it: 0x1910 bytes. That slot 1 is unused is inferred
        words.push_back({0x112D24, 0xE2842034, 0xE284203C, "the events slot 0 starts 8 bytes later (FUN_00112d0c)"});
        words.push_back({0x3DB770, 0x00000C84, 0x00001910, "Event_LoadZoneEventsFile's bound on the file's size"});
        words.push_back({0x3DB754, 0xE5806C84, 0xE5006004, "the file's size stored before the slot (str r6, [r0, #-4])"});
        words.push_back({0x3DB75C, 0xE2800B03, 0xE2400090, "the zone stored 8 bytes before the slot (sub r0, r0, #0x90; strh [r0, #0x88])"});
        words.push_back({0x3DB8B8, 0xE5950C84, 0xE5150004, "the binder reads the size before the slot (ldr r0, [r5, #-4])"});
        words.push_back({0x3DB988, 0x00000C88, 0xFFFFFFF8, "the binder reads the zone 8 bytes before the slot"});
        log.push_back("events: a zone's file up to 0x1910 bytes (the game's 0xC84), slot 0 over both slots");
    }
    std::vector<CroWord> croWords;
    if (fieldObjectHeap) croWords.push_back({0x10283954, 0xE3A02801, 0xE3A02000 | *ArmImmediate(*fieldObjectHeap), "heap 0x62 of the field objects (Field_SubsystemInit)"});
    Bytes dllField;
    if (o.CharacterModels)
    {
        // Table A and Table B above. The bound: list entries + 1 (the player's) below N + 2, as the game's 33 for 31
        const uint32_t models = *o.CharacterModels;
        if (models < 31 || models > 127) throw FormatError("character models: 31 to 127 (the moved cache holds 161 entries, 32 kept for models asked for at run time)");
        for (const ModelCacheWord& m : ModelCacheWords) words.push_back({m.Address, m.Expected, m.Value, m.Why});
        words.push_back({0x3F7300, 0xE3500021, 0xE3500000 | (models + 2), "Event_LoadCharacterArchiveMember's bound on the cached entries"});
        const uint32_t slots = (models + 2) | 1;
        words.push_back({0x3F54CC, 0x2EC8, 0x2F40 + 0xA0 * slots, "the model resource manager's size (Event_CharacterManager_Init)"});
        const Bytes cro = oras.Read("DllField.cro");
        const std::vector<CroWord> slotWords = ModelSlotWords(slots, DllFieldBase + CroLayout(cro).CodeEnd());
        croWords.insert(croWords.end(), slotWords.begin(), slotWords.end());
        snprintf(line, sizeof line, "character models: %u per area pack (the game's 31); model slots %u (manager 0x%X bytes), %zu DllField words", models, slots, 0x2F40 + 0xA0 * slots, slotWords.size());
        log.push_back(line);
    }
    if (fieldObjects)
    {
        // The models' instances (skeletons, Gfx_Skeleton_Build) come from the manager's heaps 0x159 (+0x14) and 0x15A
        // (+0x18), which Field_SubsystemInit makes with sizes read from DllField's table at 0x103369C0 / 0x103369C8 (0x100000
        // and 0xB0000 for the field's entry; 0x100000 and 0x400000 for the other) by `ldr r2, [r1, r8, lsl #2]` at
        // 0x10283970 and 0x10283994. 59 shown characters ran one out: thread 1 spun in the allocator's out-of-memory loop
        // (FUN_0012b9a0, pc 0x12BAEC, called from Gfx_Skeleton_Build; run hK, headless, read 17 October). Both grow with
        // the objects (size / 32 x objects), in a cave as the table is data; their parent, heap 0x5D, grows below
        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        for (const uint32_t at : {0x10283970u, 0x10283994u})
        {
            const char* why = at == 0x10283970 ? "heap 0x159 of the model instances (Field_SubsystemInit)" : "heap 0x15A of the model instances (Field_SubsystemInit)";
            croWords.push_back({at, 0xE7912108, Branch(at, cave, false), std::string(why) + ": to its cave"});
            const uint32_t routine[] = {
                0xE7912108,                                    // ldr r2, [r1, r8, lsl #2]   the game's size
                0xE1A022A2,                                    // lsr r2, r2, #5             / 32
                0xE3A0C000 | *ArmImmediate(*fieldObjects),     // mov r12, #objects
                0xE002029C,                                    // mul r2, r12, r2
                Branch(cave + 16, at + 4, false),              // back
            };
            for (uint32_t k = 0; k < 5; k++) croWords.push_back({cave + 4 * k, 0, routine[k], std::string(why) + " (cave)"});
            cave += 20;
        }
        log.push_back("character model instances: heaps 0x159 and 0x15A x " + std::to_string(*fieldObjects) + " / 32");
        // Their parent is the field's heap 0x5D (Event_CharacterManager_Init's param_4, the handle at field +0x2C), made by
        // FUN_00404600 from heap 0x17 with the size word 0x404768 (0x1350000), and the character models are decompressed
        // into it as they load (FUN_003f4d8c). Grown by what the two heaps grow for the field's entry (0x100000 + 0xB0000):
        // 127 characters without it ran out at boot (run h127c: the allocator's out-of-memory loop, r5 heap 0x5D, r7 0x17800)
        const uint32_t grown = ((0x1B0000u / 32 * *fieldObjects - 0x1B0000u) + 0xFFFFu) & ~0xFFFFu;
        words.push_back({0x404768, 0x01350000, 0x01350000 + grown, "heap 0x5D of the field (FUN_00404600), the parent of heaps 0x159 and 0x15A"});
        snprintf(line, sizeof line, "field heap 0x5D: 0x1350000 -> 0x%X", 0x01350000 + grown);
        log.push_back(line);
        // The field's 3D scene (FUN_00404c80, through FUN_00392ab4) has 8 model groups, their capacity in word 0x18 of
        // each 100-byte entry: 256, 32, 32, 32, 32, 64, 64, 32 (read in memory, run hMax3). Characters fill groups 5 and 6:
        // a zone of 120 stopped in Field_RegisterObjectModelsInScene's fatal with both at 64 of 64. Their 0x40 is r0, which
        // the next table reuses, so the two stores take r9 instead, which holds 0x80 there (stored as entry 5's word 0)
        if (*o.Characters + 6 > 64)
        {
            words.push_back({0x404E30, 0xE58702B8, 0xE58792B8, "scene group 6's model capacity 0x80 (str r9 for r0, FUN_00404c80)"});
            words.push_back({0x404E3C, 0xE5870254, 0xE5879254, "scene group 5's model capacity 0x80 (str r9 for r0, FUN_00404c80)"});
            log.push_back("field scene: groups 5 and 6 hold 128 models each (the game's 64)");
        }
    }
    if (o.LargeWildTables)
    {
        // X/Y's five land tables (OrasXyEncounters.h). Omega Ruby's step check (Field_WildEncounter_StepCheck) gives a tile
        // kind a table kind: 0x1F 0x20 0x24 0x25 0x2B 0x2D kind 0, 0x27 kind 1, any other no encounter; X/Y's (its DllField
        // 0x834AC, read 20 October) 0x20 0x24 0x25 table 0, 0x21-0x23 tables 1-3, any other table 4 (the zone's header byte
        // of the table 0: none). Here the kinds Omega Ruby knows keep theirs, 0x21-0x23 take kinds 9-11 and any other kind
        // 12, so Map_GetAttributeEffectEntry, knowing none of them, takes their entry from the zone file's header byte
        // [kind] - 1, as X/Y does (bytes 9-12 are zero in all 150 of Omega Ruby's encounter files: none of its zones change).
        // Field_WildEncounter_SelectTable gives kinds 9-11 the tables at +0x102, +0x132, +0x162 (12 slots, kind 0's pick)
        // and kind 12 kind 1's table, then names them kind 0 and kind 1 for the rest of the encounter.
        const auto at = [](uint32_t file) { return DllFieldBase + file; };
        const auto branch = [](uint32_t cond, uint32_t from, uint32_t to) { return (Branch(from, to, false) & 0x0FFFFFFFu) | cond << 28; };
        constexpr uint32_t EQ = 0x0, HI = 0x8, AL = 0xE;
        const uint32_t step[14] = {
            0xE3500025, 0x1350001F, 0x13500020, 0x13500024, 0x1350002B, 0x1350002D, // cmp r0, #0x25; cmpne #0x1F ... #0x2D
            branch(EQ, at(0x8B420), at(0x8B440)),                                   // beq: kind 0
            0xE3500027,                                                             // cmp r0, #0x27
            branch(EQ, at(0x8B428), at(0x8B444)),                                   // beq: kind 1 (r4 holds 1)
            0xE2401021, 0xE3510002, 0x92814009, 0x83A0400C,                         // sub r1, r0, #0x21; cmp r1, #2; addls r4, r1, #9; movhi r4, #12
            branch(AL, at(0x8B43C), at(0x8B444)),                                   // b: kind 9-12
        };
        const uint32_t stepGame[14] = {0xE3500025, 0xE320F000, 0x0A00000A, 0xCA000004, 0xE350001F, 0x13500020, 0x13500024,
                                       0x0A000005, 0xEA000049, 0xE3500027, 0x0A000003, 0xE350002B, 0x1350002D, 0x1A000044};
        for (uint32_t k = 0; k < 14; k++) croWords.push_back({at(0x8B408 + 4 * k), stepGame[k], step[k], "Field_WildEncounter_StepCheck's tile kinds"});

        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        // Field_WildEncounter_TileEntry: its "none" (0xFF) for an unknown kind, the same kinds as the step check
        croWords.push_back({at(0x97D50), 0xE3A080FF, Branch(at(0x97D50), cave, false), "Field_WildEncounter_TileEntry's unknown kind: to its cave"});
        const uint32_t entry[5] = {0xE240C021, 0xE35C0002, 0x928C8009, 0x83A0800C, Branch(cave + 16, at(0x97D54), false)};
        for (uint32_t k = 0; k < 5; k++) croWords.push_back({cave + 4 * k, 0, entry[k], "Field_WildEncounter_TileEntry (cave)"});
        cave += 20;
        // Field_WildEncounter_SelectTable's default (kind 9 and up): r1 the file, r2 the kind, r3 1, r4 the context
        croWords.push_back({at(0x7DBC8), 0xEA000023, Branch(at(0x7DBC8), cave, false), "Field_WildEncounter_SelectTable's kinds past 8: to its cave"});
        const uint32_t tail = cave + 15 * 4;
        const uint32_t select[20] = {
            0xE352000C, 0x0281103E, 0x05C43082, 0x05C43080,  // cmp r2, #12; addeq r1, r1, #0x3E; strbeq r3, [r4, #0x82]; strbeq r3, [r4, #0x80]
            branch(EQ, cave + 16, tail),                     // beq: the 12-slot tail
            0xE242C009, 0xE35C0002,                          // sub ip, r2, #9; cmp ip, #2
            branch(HI, cave + 28, at(0x7DC5C)),              // bhi: the game's "no table"
            0xE081128C, 0xE081120C, 0xE2811C01, 0xE2811002,  // r1 += 0x30 (kind - 9) + 0x102
            0xE3A00000, 0xE5C40082, 0xE5C40080,              // mov r0, #0; strb r0, [r4, #0x82]; strb r0, [r4, #0x80]
            0xE3A0000C, 0xE5843074, 0xE5840070, 0xE584306C,  // tail: 12 slots (the game's table 0x141F20 for kinds 0 and 1), as the game's
            Branch(cave + 19 * 4, at(0x7DC88), false),       // back to the slot pick
        };
        for (uint32_t k = 0; k < 20; k++) croWords.push_back({cave + 4 * k, 0, select[k], "Field_WildEncounter_SelectTable (cave)"});
        log.push_back("wild tables: kinds 9-12 (X/Y's land tables 1-4) on tiles 0x21-0x23 and kinds Omega Ruby has none for");
    }
    if (!o.XyWildEffects.empty())
    {
        // X/Y's wild battle types (read 22 October, ORAS_ENGINE.md 0.1). A step encounter's battle type is the encounter effect
        // entry's byte +4 (+5 for a horde): FUN_103279DC reads it when the context's type is 0x49 (X/Y's FUN_102DD9C4: 0x40),
        // the type the step check gives. The entry (6 bytes: the step rate's growth, steps per growth, maximum, steps before
        // any, then the two types) is Map_GetAttributeEffectEntry's: Omega Ruby's 12 at DllField 0xF308C, chosen by the tile
        // kind (the zone file's header byte [kind] - 1 for a kind it does not list); X/Y's 12 at its DllField 0xEF7D0, always
        // the header byte [table] - 1. X/Y's differ (their rates, entry 4, and their types), so they are appended here, after
        // the cave's code, and X/Y's zones name them by header bytes 13-24 (OrasXyEncounters.h), which the lookup takes first
        // (13 and up: Omega Ruby's own bytes are 12 at most, 150 files read). Fishing and Rock Smash give fixed types: Omega
        // Ruby's 3 and 9 (Field_WildEncounter_TryKind, Field_WildEncounter_StartRockSmash), X/Y's 5 and 10 (its fishing
        // FUN_1029A890, its CallWildBattleRockCrash FUN_1029B120), taken in a zone whose file's byte 13 is 1 (X/Y's)
        if (!o.LargeWildTables || o.XyBattleTypes.empty()) throw FormatError("--xy-wild-effects needs --wild-tables large and --xy-battle-types");
        const Bytes effects = ReadFile(o.XyWildEffects);
        if (effects.size() != 12 * 6) throw FormatError("--xy-wild-effects: 12 entries of 6 bytes");
        constexpr uint32_t gameTypes = 73, xFishing = gameTypes + 5, xRockSmash = gameTypes + 10;
        if (gameTypes + ReadFile(o.XyBattleTypes).size() / 24 <= xRockSmash) throw FormatError("--xy-wild-effects: X/Y's types 5 and 10 not in --xy-battle-types");
        const auto at = [](uint32_t file) { return DllFieldBase + file; };
        const auto branch = [](uint32_t cond, uint32_t from, uint32_t to) { return (Branch(from, to, false) & 0x0FFFFFFFu) | cond << 28; };
        constexpr uint32_t EQ = 0x0, LS = 0x9;
        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        const uint32_t table = cave;
        for (uint32_t k = 0; k < 18; k++)
            croWords.push_back({cave + 4 * k, 0, (uint32_t)effects[4 * k] | (uint32_t)effects[4 * k + 1] << 8 | (uint32_t)effects[4 * k + 2] << 16 | (uint32_t)effects[4 * k + 3] << 24,
                                 "X/Y's wild encounter effect entries (cave)"});
        cave += 72;
        // Map_GetAttributeEffectEntry (r1 the tile, r2 the kind or 0xFF, r3 the zone's encounter file, never 0 at its three
        // callers): a header byte of 13 and up is X/Y's entry (byte - 13), else the game's. The entries are reached from the
        // pc (the module runs elsewhere than its linked address, and a word of the cave has no relocation)
        croWords.push_back({at(0x7DFE8), 0xE92D41F0, Branch(at(0x7DFE8), cave, false), "Map_GetAttributeEffectEntry: to its cave"});
        const uint32_t lookup[12] = {
            0xE92D41F0,                             // stmdb sp!, {r4-r8, lr} (the game's)
            0xE35200FF, branch(EQ, cave + 8, cave + 44), // cmp r2, #0xFF; beq: the game's
            0xE7D3C002, 0xE35C000C,                 // ldrb ip, [r3, r2]; cmp ip, #12
            branch(LS, cave + 20, cave + 44),       // bls: the game's
            0xE24CC00D, 0xE08CC08C,                 // sub ip, ip, #13; add ip, ip, ip, lsl #1
            0xE24F0070, 0xE080008C,                 // sub r0, pc, #112 (the entries, 72 bytes before this code); add r0, r0, ip, lsl #1
            0xE8BD81F0,                             // ldmia sp!, {r4-r8, pc}
            Branch(cave + 44, at(0x7DFEC), false),  // the game's: back after its first instruction
        };
        if (cave + 32 + 8 - 112 != table) throw FormatError("--xy-wild-effects: the entries are not where the lookup reads them");
        for (uint32_t k = 0; k < 12; k++) croWords.push_back({cave + 4 * k, 0, lookup[k], "Map_GetAttributeEffectEntry (cave)"});
        cave += 12 * 4;
        // the fixed types, r5 the zone's encounter file at both: mov r2, #type made a call
        for (const auto& [site, game, xy, why] : {std::tuple<uint32_t, uint32_t, uint32_t, const char*>{0x97EC4, 3, xFishing, "Field_WildEncounter_TryKind (fishing)"},
                                                  {0x97F88, 9, xRockSmash, "Field_WildEncounter_StartRockSmash"}})
        {
            croWords.push_back({at(site), 0xE3A02000 | game, Branch(at(site), cave, true), std::string(why) + ": its battle type from the cave"});
            const uint32_t pick[5] = {0xE5D5200D, 0xE3520000, 0x03A02000 | game, 0x13A02000 | xy, 0xE12FFF1E}; // ldrb r2, [r5, #13]; cmp; moveq; movne; bx lr
            for (uint32_t k = 0; k < 5; k++) croWords.push_back({cave + 4 * k, 0, pick[k], std::string(why) + "'s battle type (cave)"});
            cave += 20;
        }
        snprintf(line, sizeof line, "wild battles: X/Y's 12 encounter effect entries at 0x%X, its fishing and Rock Smash types %u and %u in its zones", table, xFishing, xRockSmash);
        log.push_back(line);
    }
    if (o.XyTrainers)
    {
        // the trainer flag routine above, in DllField's cave, for its three trainer flags (the one at 0x2133C after its bound)
        const auto at = [](uint32_t file) { return DllFieldBase + file; };
        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        croWords.push_back({at(0x21328), 0xE3500FF5, 0xE3500C07, "DllField's trainer flag bound (980 -> 1792)"});
        for (const uint32_t site : {0x2133Cu, 0xDFC34u, 0xDFDECu})
        {
            croWords.push_back({at(site), 0xE2800B01, 0xE320F000, "a trainer flag: the id kept in r0 (nop)"});
            croWords.push_back({at(site + 4), 0xE2800FB3, Branch(at(site + 4), cave, true), "a trainer flag from the routine"});
        }
        for (uint32_t k = 0; k < 12; k++) croWords.push_back({cave + 4 * k, 0, TrainerFlagRoutine[k], "trainer flags (cave)"});
    }
    if (std::any_of(o.ScriptMasks.begin(), o.ScriptMasks.end(), [](const auto& m) { return (m.second & XyNativesBit) != 0; }))
    {
        // X/Y's own natives (OrasXyNatives.h), for the zones whose mask holds their bit
        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        const XyNativesPatch p = XyNativesPatchAt(cave);
        words.insert(words.end(), p.Code.begin(), p.Code.end());
        for (const CodeWord& w : p.Field) croWords.push_back({w.Address, w.Expected, w.Value, w.Why});
        log.push_back("X/Y natives: " + std::to_string(XyNatives().size()) + " under mask bit 0x800");
    }
    if (o.XyMessages)
    {
        // MsgLoad(table, n) (Script_Native_MsgLoad 0x1029EF60, read 20 October) takes table 8's member from a switch of n
        // 0-24 (0 -> 0x104, 21 -> 0x61...), past it the error routine then member 0 (0x1029F110-0x1029F120). X/Y's switch
        // is its own (its n 21, the nickname list, is its member 0x4F), so X/Y's files are appended to the archives and a
        // script names one as 0x100 + its member: the out-of-range branch made "sub r7, r7, #0x100", its member
        const uint32_t game[5] = {0xE3A02000, 0xE1A01002, 0xE1A00002, 0xEBFE8C83, 0xE3A07000};
        croWords.push_back({0x1029F110, game[0], 0xE2477C01, "MsgLoad: n >= 0x19 is the member n - 0x100 (sub r7, r7, #0x100)"});
        for (uint32_t k = 1; k < 5; k++) croWords.push_back({0x1029F110 + 4 * k, game[k], 0xE320F000, "MsgLoad: its error call left out (nop)"});
        log.push_back("MsgLoad: 0x100 + m loads table 8's member m");
    }
    std::vector<std::pair<std::string, Bytes>> gymModules; // X/Y's gym modules ported, written into the mod's romfs below
    std::vector<std::pair<uint32_t, Bytes>> gymArchives;   // their archives: Omega Ruby's new number, X/Y's file
    if (!o.XyGyms.empty())
    {
        // X/Y's gym modules (OrasXyGymModules.h): each ported from X/Y's image, and DllField loading it by its area kind
        N3dsRom xy(o.XyGyms);
        const XyGameModules x{xy.Read("static.crs"), xy.Read("DllField.cro"), 0x101F9000};
        const XyGameModules orasModules{oras.Read("static.crs"), oras.Read("DllField.cro"), DllFieldBase};
        const XySounds sounds = XySounds::Read(oras.Code(), oras.Read("sound/sango_sound.bcsar"), xy.Code(), xy.Read("sound/xy_sound.bcsar"));
        // each module once (Clemont's serves three kinds), its archive given the next number past the game's (the path
        // table below, in the room)
        std::set<std::string> ported;
        for (const XyGym& g : XyGyms())
        {
            if (!ported.insert(g.Port.Name).second) continue;
            uint32_t archive = 0;
            if (g.Port.Archive)
            {
                archive = XyGymArchiveNumber(gymArchives.size());
                std::vector<std::string> report;
                gymArchives.push_back({archive, XyGymArchiveToOras(xy.Read(XyArchivePath(g.Port.Archive)), report)});
                for (const std::string& r : report) log.push_back(g.Port.Name + "'s archive: " + r);
            }
            gymModules.push_back({g.Port.Name + ".cro", XyPortModule(xy.Read(g.Port.XName + ".cro"), x, orasModules, g.Port,
                                                                    [&](uint32_t id) { return sounds.Translate(id); }, archive)});
        }
        const CroLayout layout(oras.Read("DllField.cro"));
        uint32_t cave = DllFieldBase + layout.CodeEnd();
        for (const CroWord& w : croWords) if (w.Address >= cave) cave = w.Address + 4;
        for (const CodeWord& w : XyGymLoaderPatchAt(cave)) croWords.push_back({w.Address, w.Expected, w.Value, w.Why});
        log.push_back("X/Y gyms: " + std::to_string(XyGyms().size()) + " module(s) ported and loaded by their area kinds");
    }
    if (!croWords.empty()) dllField = PatchCro(oras.Read("DllField.cro"), DllFieldBase, croWords);
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

    if (o.XySounds)
    {
        // X/Y's streams appended to the sound archive (OrasXySound.h), sounds first + k, played by ids k + 1 of category 0:
        // its entry of the category table (0x592728, read by FUN_00115984: a count, then the list's address; 0 and 0 in the
        // game) and the list, in the rodata segment's last page past its 0x71A14 bytes (0x5EBA14-0x5EC000, zeros, mapped
        // with the segment as the .code's cave at 0x579608 is). Id 0 the game's silence, never given
        const auto [first, count] = *o.XySounds;
        constexpr uint32_t list = 0x5EBA18, end = 0x5EC000, categories = 0x592728;
        if (count == 0 || list + 4 * (count + 1) > end) throw FormatError("--xy-sounds: 1 to " + std::to_string((end - list) / 4 - 1) + " sounds");
        words.push_back({categories, 0, count + 1, "sound category 0's count"});
        words.push_back({categories + 4, 0, list, "sound category 0's list"});
        words.push_back({list, 0, 0x01000000, "sound category 0, id 0: the game's silence"});
        for (uint32_t k = 0; k < count; k++) words.push_back({list + 4 * (k + 1), 0, 0x01000000 | (first + k), "sound category 0: X/Y's stream " + std::to_string(k)});
        snprintf(line, sizeof line, "sound ids: category 0 holds %u X/Y streams, sounds %u-%u", count, first, first + count - 1);
        log.push_back(line);
    }

    if (o.StartZone)
    {
        // A new game's first zone (read 20 October): FUN_003D4F14 builds the player's location with FUN_00403D80(location,
        // zone, heap), which stores the zone and takes the position from that zone's header (its spawn, shorts +0x32-+0x36);
        // the zone is the literal 399 (the moving truck) at 0x3D4F98, its only use. Measured: a new game's Situation (save
        // block 4) holds zone 399 at tile (4.5, 4.5) once in the truck (run ng9). Not read: other code that expects the
        // truck (the intro's own scenes run from zone 399's scripts, inferred)
        if (*o.StartZone >= 0x10000) throw FormatError("--start-zone: a zone number below 65536");
        words.push_back({0x3D4F98, 399, *o.StartZone, "a new game's first zone (FUN_003D4F14's literal)"});
        log.push_back("new game: starts in zone " + std::to_string(*o.StartZone));
    }

    // The room (read 20 October): memory past the game's .bss, mapped from boot, for tables that outgrow their place. Azahar
    // sizes the data segment as its pages plus the .bss's, and resizes the code buffer to include the .bss before it applies
    // code.ips (azahar/src/core/loader/ncch.cpp), so a larger .bss size in the exheader gives pages after the game's own
    // .bss (0x6AF000 up in Omega Ruby: data at 0x5EC000, 68 pages, .bss 0x7EC2C) that code.ips can fill; the game's start
    // clears only its own .bss (inferred: its end is fixed in the code). Azahar only: a console reads the exheader signed
    // NCCH's, not the mod's
    const Bytes exheader = oras.ExHeader();
    const auto u32 = [&](size_t at) { return (uint32_t)(exheader.at(at) | exheader.at(at + 1) << 8 | exheader.at(at + 2) << 16 | (uint32_t)exheader.at(at + 3) << 24); };
    const uint32_t dataEnd = u32(0x30) + u32(0x34) * 0x1000, gameBss = u32(0x3C);
    const uint32_t roomBase = dataEnd + ((gameBss + 0xFFF) & ~0xFFFu);
    uint32_t roomAt = roomBase;
    const auto roomPut = [&](const Bytes& data, const std::string& why) {
        const uint32_t at = roomAt;
        for (size_t k = 0; k < data.size(); k += 4)
        {
            uint32_t v = 0;
            for (size_t b = 0; b < 4 && k + b < data.size(); b++) v |= (uint32_t)data[k + b] << (8 * b);
            words.push_back({at + (uint32_t)k, 0, v, why + " (room)"});
        }
        roomAt += (uint32_t)((data.size() + 3) & ~(size_t)3);
        return at;
    };
    if (!o.XyBattleTypes.empty())
    {
        // The battle types (read 20 October): 24-byte records from 0x57A014 (rodata), the battle's music (+0x10) and victory
        // jingle (+0x14) among them, a battle's record the byte at its settings' +0x10 (from the trainer class's byte +2,
        // a/0/3/7), base + byte x 24 with no bound (15 readers: the literal words below, each 0x57A014). 73 in the game; the
        // copy in the room keeps them at the same numbers and adds the file's after them (X/Y's, OrasXyTrainers.h); code
        // reading one fixed record by its own address reads the game's copy, unchanged
        constexpr uint32_t table = 0x57A014, gameTypes = 73;
        const Bytes extra = ReadFile(o.XyBattleTypes);
        if (extra.empty() || extra.size() % 24 || gameTypes + extra.size() / 24 > 256) throw FormatError("--xy-battle-types: 24-byte records, up to 183");
        Bytes all(code.begin() + (table - CodeBase), code.begin() + (table - CodeBase) + gameTypes * 24);
        all.insert(all.end(), extra.begin(), extra.end());
        const uint32_t moved = roomPut(all, "the battle types");
        for (const uint32_t pool : {0x1D6C70u, 0x1D6E08u, 0x1D6F44u, 0x1D8D3Cu, 0x1DBCF0u, 0x1DBE64u, 0x1DC07Cu, 0x1DF638u, 0x1E0514u,
                                    0x1E0630u, 0x1E07B8u, 0x1E0A1Cu, 0x1E0DE0u, 0x1E15DCu, 0x1E1C14u})
            words.push_back({pool, table, moved, "a battle type reader's table"});
        snprintf(line, sizeof line, "battle types: %zu after the game's 73, the table at 0x%X", extra.size() / 24, moved);
        log.push_back(line);
    }
    // the gyms' archives (OrasXyGymModules.h: numbers past 299 that are ARM immediates, 0x130...) take this table too, as
    // many more as their highest number needs
    uint32_t moreArchives = o.MoreArchives;
    for (const auto& [number, file] : gymArchives) moreArchives = std::max(moreArchives, number + 1 - 299);
    if (moreArchives)
    {
        // More archives (read 20 October): the archive paths ("rom:/a/0/0/0", UTF-16) are 299 pointers from 0x5F5050, the
        // table and its count read at boot from the literal words 0x1085BC and 0x1085B8 (stored into 0x6173F0 + 0x54 and
        // + 0x50 by 0x10A49C; no other word points into the table). The copy in the room keeps the game's pointers and adds
        // the new paths after them, archive n at rom:/a/<n's three digits>, the files themselves added under romfs/
        constexpr uint32_t table = 0x5F5050, countWord = 0x1085B8, tableWord = 0x1085BC, gameArchives = 299;
        const uint32_t count = gameArchives + moreArchives;
        if (count > 1000) throw FormatError("--more-archives: up to 701 (paths have three digits)");
        const uint32_t at = roomAt, strings = at + count * 4;
        Bytes all(code.begin() + (table - CodeBase), code.begin() + (table - CodeBase) + gameArchives * 4), text;
        for (uint32_t n = gameArchives; n < count; n++)
        {
            const uint32_t p = strings + (uint32_t)text.size();
            for (int b = 0; b < 4; b++) all.push_back((uint8_t)(p >> (8 * b)));
            char path[16];
            snprintf(path, sizeof path, "rom:/a/%u/%u/%u", n / 100, n / 10 % 10, n % 10);
            for (const char* c = path;; c++) { text.push_back((uint8_t)*c); text.push_back(0); if (!*c) break; }
        }
        all.insert(all.end(), text.begin(), text.end());
        if (roomPut(all, "the archive paths") != at) throw FormatError("--more-archives: the room moved");
        words.push_back({tableWord, table, at, "the archive path table"});
        words.push_back({countWord, gameArchives, count, "the archive count"});
        snprintf(line, sizeof line, "archives: %u after the game's 299 (a/2/9/9 up), the path table at 0x%X", moreArchives, at);
        log.push_back(line);
    }

    Bytes dllBattle, dllBackGround; // the modules patched (empty: the game's)
    if (!o.XyBackgrounds.empty())
    {
        // The battle backgrounds' tables, longer (OrasXyBackgrounds.h): each copied into the room, the game's entries first,
        // and its readers pointed at the copy, each read through one literal word: in the .code the records (2 bytes a
        // value, then 5 at +2N: FUN_0048AA44's `add r1, ip, #0xBA` made +0x200, 256 values), the scenery sets and sceneries
        // (their static copies are the same as DllBackGround's: one copy for both); in DllBackGround the backdrops' members
        // and two light tables, the grounds', the sets and the sceneries (two readers); in DllBattle the platforms'. The
        // modules' literals hold the room's address (PointLiteralAt)
        const Bytes file = ReadFile(o.XyBackgrounds);
        if (file.size() < 4 || std::string(file.begin(), file.begin() + 4) != "XYBG") throw FormatError("--xy-backgrounds: not an XYBG file");
        std::vector<Bytes> t;
        for (size_t at = 4; at < file.size();)
        {
            const uint32_t size = U32(file, at);
            if (at + 4 + size > file.size()) throw FormatError("--xy-backgrounds: a table past the file's end");
            t.emplace_back(file.begin() + at + 4, file.begin() + at + 4 + size);
            at += 4 + size;
        }
        if (t.size() != 8 || t[0].size() != 7 * 256) throw FormatError("--xy-backgrounds: 8 tables, the records 256 values");
        const Bytes& records = t[0];
        const Bytes& sets = t[1];
        const Bytes& sceneries = t[2];
        dllBackGround = oras.Read("DllBackGround.cro");
        dllBattle = oras.Read("DllBattle.cro");
        // each table must begin with the game's own
        const auto game = [&](const Bytes& from, uint32_t at, const Bytes& table, size_t size, const char* what) {
            if (table.size() < size || !std::equal(table.begin(), table.begin() + size, from.begin() + at)) throw FormatError(std::string("--xy-backgrounds: ") + what + " do not begin with the game's");
        };
        constexpr uint32_t recordsAt = 0x595CD0, setsAt = 0x595F5C, sceneriesAt = 0x595F68, bgBase = 0x10000000, battleBase = 0x10025000;
        game(code, recordsAt - CodeBase, Bytes(records.begin(), records.begin() + 2 * 93), 2 * 93, "the records");
        game(code, recordsAt - CodeBase + 2 * 93, Bytes(records.begin() + 2 * 256, records.begin() + 2 * 256 + 5 * 93), 5 * 93, "the records");
        game(code, setsAt - CodeBase, sets, 12, "the scenery sets");
        game(code, sceneriesAt - CodeBase, sceneries, 0x18, "the sceneries");
        game(dllBackGround, 0x100075C8 - bgBase, sets, 12, "the scenery sets");
        game(dllBackGround, 0x100075D4 - bgBase, sceneries, 0x18, "the sceneries");
        game(dllBackGround, 0x10005000 - bgBase, t[3], 4 * 93, "the backdrops");
        game(dllBackGround, 0x10005174 - bgBase, t[4], 0x30 * 93, "the backdrops' first lights");
        game(dllBackGround, 0x100062E4 - bgBase, t[5], 0x30 * 93, "the backdrops' second lights");
        game(dllBackGround, 0x10007454 - bgBase, t[6], 4 * 93, "the grounds");
        game(dllBattle, 0x10106074 - battleBase, t[7], 4 * 92, "the platforms");

        const uint32_t recordsRoom = roomPut(records, "the background records"), setsRoom = roomPut(sets, "the scenery sets"),
                       sceneriesRoom = roomPut(sceneries, "the sceneries");
        words.push_back({0x48AC0C, recordsAt, recordsRoom, "FUN_0048AA44's background records"});
        words.push_back({0x48AA5C, 0xE28C10BA, 0xE28C1000 | *ArmImmediate(2 * 256), "FUN_0048AA44's records' second part (add r1, ip, #0x200)"});
        words.push_back({0x48AC14, setsAt, setsRoom, "FUN_0048AA44's scenery sets"});
        words.push_back({0x48AC18, sceneriesAt, sceneriesRoom, "FUN_0048AA44's sceneries"});
        const uint32_t backdrops = roomPut(t[3], "the backdrops"), lightA = roomPut(t[4], "the backdrops' first lights"), lightB = roomPut(t[5], "the backdrops' second lights"),
                       grounds = roomPut(t[6], "the grounds"), platforms = roomPut(t[7], "the platforms");
        PointLiteralAt(dllBackGround, bgBase, 0x100045A8, 0x10005000, backdrops, "DllBackGround's backdrops");
        PointLiteralAt(dllBackGround, bgBase, 0x100041A0, 0x10005174, lightA, "DllBackGround's first lights");
        PointLiteralAt(dllBackGround, bgBase, 0x100041A4, 0x100062E4, lightB, "DllBackGround's second lights");
        PointLiteralAt(dllBackGround, bgBase, 0x100045AC, 0x10007454, grounds, "DllBackGround's grounds");
        PointLiteralAt(dllBackGround, bgBase, 0x100045B0, 0x100075C8, setsRoom, "DllBackGround's scenery sets");
        PointLiteralAt(dllBackGround, bgBase, 0x100045B4, 0x100075D4, sceneriesRoom, "DllBackGround's sceneries");
        PointLiteralAt(dllBackGround, bgBase, 0x10001B04, 0x100075D4, sceneriesRoom, "DllBackGround's sceneries (second reader)");
        PointLiteralAt(dllBattle, battleBase, 0x10070FA8, 0x10106074, platforms, "DllBattle's platforms");
        snprintf(line, sizeof line, "battle backgrounds: the tables in the room (records 0x%X, %zu backdrops, %zu grounds, %zu platforms, %zu scenery sets, %zu sceneries)",
                 recordsRoom, t[3].size() / 4, t[6].size() / 4, t[7].size() / 4, sets.size() / 12, sceneries.size() / 0x18);
        log.push_back(line);
    }

    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = std::filesystem::path(outDir) / "load" / "mods" / id;
    if (o.Memory || roomAt != roomBase)
    {
        Bytes header = o.Memory ? ExHeaderWithSystemMode(exheader, *o.Memory) : exheader;
        if (roomAt != roomBase)
        {
            const uint32_t bss = roomAt - dataEnd; // the game's .bss (page-rounded) and the room
            for (int b = 0; b < 4; b++) header[0x3C + b] = (uint8_t)(bss >> (8 * b));
            snprintf(line, sizeof line, "room: 0x%X-0x%X (%u bytes past the game's .bss)", roomBase, roomAt, roomAt - roomBase);
            log.push_back(line);
        }
        std::filesystem::create_directories(root);
        WriteFile((root / "exheader.bin").string(), header);
        log.push_back(std::string("exheader.bin:") + (o.Memory ? " system mode " + std::to_string((int)*o.Memory) : "") + (roomAt != roomBase ? " .bss with the room" : ""));
    }
    if (!words.empty())
    {
        // the code as Azahar patches it: its .bss and the room after it (zeros)
        Bytes image = code;
        if (roomAt != roomBase) image.resize(roomAt - CodeBase, 0);
        const Bytes ips = CodePatchIps(image, words);
        CodePatchApply(image, ips);
        std::filesystem::create_directories(root / "exefs");
        WriteFile((root / "exefs" / "code.ips").string(), ips);
        log.push_back("code.ips: " + std::to_string(words.size()) + " words");
    }
    for (const auto& [number, file] : gymArchives)
    {
        // a new archive (X/Y's, whole) at its new number's path
        const std::filesystem::path path = root / "romfs" / XyArchivePath(number);
        std::filesystem::create_directories(path.parent_path());
        WriteFile(path.string(), file);
        log.push_back("romfs/" + XyArchivePath(number) + ": " + std::to_string(file.size()) + " bytes (X/Y's)");
    }
    for (const auto& [name, module] : gymModules)
    {
        // a module of its own (X/Y's, ported): the whole file, under a name the game has no file of
        std::filesystem::create_directories(root / "romfs");
        WriteFile((root / "romfs" / name).string(), module);
        log.push_back("romfs/" + name + ": " + std::to_string(module.size()) + " bytes");
    }
    if (o.Badges16)
    {
        // The badge case (DllTrainerCase, linked at 0x1081E000; read 21 October): FUN_1081EC74 keeps the badge count at
        // this + 0xA8 and lights badge i of the 8 when i < count (0x1081EE18), and its detail (FUN_108202B0, 0x10820310)
        // gives badge i's name text when i < count, else the unknown badge's; this + 0xA8 has no other reader. Each badge's
        // own bit now (badges stay their region's own, owner, 21 October): the badge word kept instead of the count, and
        // both tests made "bit i set" (the case shows Hoenn's 0-7; Kalos's 8-15 are not in its 8)
        constexpr uint32_t base = 0x1081E000;
        const auto bl = [](uint32_t from, uint32_t to) { return Branch(from, to, true); };
        const std::vector<CroWord> caseWords = {
            {0x1081EC90, 0xEBFFFD5E, 0xE5900014, "the badge word kept, not the count (ldr r0, [r0, #0x14])"},
            {0x1081EE18, 0xE1550009, 0xE1A01539, "badge i lit when its bit is set: lsr r1, sb, r5"},
            {0x1081EE1C, 0xE1A00004, 0xE3110001, "tst r1, #1"},
            {0x1081EE20, 0x2A000007, 0xE1A00004, "mov r0, r4"},
            {0x1081EE24, 0xEBFFFD0B, 0x0A000007, "beq to the unlit badge (0x1081EE48)"},
            {0x1081EE28, 0xE3A03001, bl(0x1081EE28, 0x1081E258), "the lit badge's calls, one word down"},
            {0x1081EE2C, 0xE7962105, 0xE3A03001, "mov r3, #1"},
            {0x1081EE30, 0xE1A01003, 0xE7962105, "ldr r2, [r6, r5, lsl #2]"},
            {0x1081EE34, 0xEBFFFD09, 0xE1A01003, "mov r1, r3"},
            {0x1081EE38, 0xE320F000, bl(0x1081EE38, 0x1081E260), "bl"},
            {0x10820308, 0xE59410A8, 0xE3A01002, "the detail's call first (mov r1, #2)"},
            {0x1082030C, 0xE0850185, 0xE1A00004, "mov r0, r4"},
            {0x10820310, 0xE1510005, bl(0x10820310, 0x10820054), "bl"},
            {0x10820314, 0xE0890100, 0xE59410A8, "then badge i's bit: ldr r1, [r4, #0xA8]"},
            {0x10820318, 0xE3A01002, 0xE1A01531, "lsr r1, r1, r5"},
            {0x1082031C, 0x85905010, 0xE3110001, "tst r1, #1"},
            {0x10820320, 0x95905014, 0xE0850185, "add r0, r5, r5, lsl #3"},
            {0x10820324, 0xE1A00004, 0xE0890100, "add r0, sb, r0, lsl #2"},
            {0x10820328, 0xEBFFFF49, 0x15905010, "its name when set (ldrne)"},
            {0x1082032C, 0xE1A00004, 0x05905014, "the unknown badge's when not (ldreq)"},
            {0x10820330, 0xE320F000, 0xE1A00004, "mov r0, r4"}};
        const Bytes original = oras.Read("DllTrainerCase.cro"), patched = PatchCro(original, base, caseWords), bps = BpsCreate(original, patched);
        if (BpsApply(original, bps) != patched) throw FormatError("DllTrainerCase.cro: the patch does not rebuild the module");
        std::filesystem::create_directories(root / "romfs_ext");
        WriteFile((root / "romfs_ext" / "DllTrainerCase.cro.bps").string(), bps);
        log.push_back("romfs_ext/DllTrainerCase.cro.bps: the badge case by each badge's bit, " + std::to_string(bps.size()) + " bytes, checked");
    }
    if (!o.XyIntros.empty())
    {
        // The battle intros (read 22 October, ORAS_ENGINE.md 0.1): the intro id (a battle type's word +4) indexes a table
        // of 162 a/0/3/4 sequence numbers in DllBattle (0x10105CDC, rodata), read by FUN_10068370 through one literal word
        // (0x10069278, set by the loader: internal relocation to segment 1 + 0x8CDC); FUN_10081314 plays a sequence below
        // 0x3D0 (the archive's 976 members; cmp r1, #0x3D0 at 0x10081410) and stops the game on a larger one. Here the
        // table is copied into the free bytes past the module's code (the code's last page, zero, loaded with the file),
        // the file's sequences after the game's 162 (ids 162 up), the literal's relocation pointed at the copy (the code is
        // not changed), and the bound raised past the highest sequence given
        constexpr uint32_t base = 0x10025000, table = 0x10105CDC, literal = 0x10069278, boundAt = 0x10081410, gameIntros = 162;
        const Bytes extra = ReadFile(o.XyIntros);
        if (extra.empty() || extra.size() % 4) throw FormatError("--xy-intros: u32 sequence numbers");
        const Bytes original = dllBattle.empty() ? oras.Read("DllBattle.cro") : dllBattle;
        const CroLayout layout(original);
        const uint32_t cave = (layout.CodeEnd() + 3) & ~3u, pagesEnd = (layout.CodeEnd() + 0xFFF) & ~0xFFFu;
        const uint32_t count = gameIntros + (uint32_t)extra.size() / 4;
        if (cave + 4 * count > pagesEnd) throw FormatError("--xy-intros: the intro table does not fit past DllBattle's code");
        std::vector<CroWord> battleWords;
        uint32_t highest = 0x3CF;
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t v = i < gameIntros ? layout.U32(table - base + 4 * i) : (uint32_t)(extra[4 * (i - gameIntros)] | extra[4 * (i - gameIntros) + 1] << 8 |
                                                                                                   extra[4 * (i - gameIntros) + 2] << 16 | (uint32_t)extra[4 * (i - gameIntros) + 3] << 24);
            if (i >= gameIntros) highest = std::max(highest, v);
            battleWords.push_back({base + cave + 4 * i, 0, v, "the intro table's copy"});
        }
        // the bound: the smallest ARM immediate past the highest sequence (no intro names the numbers between)
        uint32_t limit = highest + 1;
        while (!ArmImmediate(limit) && limit < 0x10000) limit++;
        const auto bound = ArmImmediate(limit);
        if (!bound) throw FormatError("--xy-intros: no sequence bound an ARM immediate");
        battleWords.push_back({boundAt, 0xE3510E3D, 0xE3510000 | *bound, "the sequences played (cmp r1, #bound)"});
        Bytes patched = PatchCro(original, base, battleWords);
        // the literal's internal relocation: {tag (segment | offset << 4), type, symbol segment, 2 bytes, addend}
        const uint32_t relocations = layout.Field(42), relocationCount = layout.Field(43), at = literal - base - layout.Segments[0].first;
        bool moved = false;
        for (uint32_t i = 0; i < relocationCount && !moved; i++)
        {
            const uint32_t entry = relocations + 12 * i, tag = layout.U32(entry);
            if ((tag & 0xF) != 0 || (tag >> 4) != at) continue;
            if (original[entry + 4] != 2 || original[entry + 5] != 1 || layout.U32(entry + 8) != table - base - layout.Segments[1].first)
                throw FormatError("DllBattle.cro: the intro table's relocation is not the one read (another version of the game?)");
            const uint32_t addend = cave - layout.Segments[0].first;
            patched[entry + 5] = 0;
            for (int k = 0; k < 4; k++) patched[entry + 8 + k] = (uint8_t)(addend >> (8 * k));
            moved = true;
        }
        if (!moved) throw FormatError("DllBattle.cro: no relocation of the intro table's literal word");
        dllBattle = patched;
        snprintf(line, sizeof line, "DllBattle: the battle intros 162-%u (sequences up to %u)", count - 1, highest);
        log.push_back(line);
    }
    for (const auto& [name, module] : {std::pair<const char*, const Bytes*>{"DllBattle.cro", &dllBattle}, {"DllBackGround.cro", &dllBackGround}})
    {
        if (module->empty()) continue;
        const Bytes original = oras.Read(name), bps = BpsCreate(original, *module);
        if (BpsApply(original, bps) != *module) throw FormatError(std::string(name) + ": the patch does not rebuild the module");
        std::filesystem::create_directories(root / "romfs_ext");
        WriteFile((root / "romfs_ext" / (std::string(name) + ".bps")).string(), bps);
        log.push_back("romfs_ext/" + std::string(name) + ".bps: " + std::to_string(bps.size()) + " bytes, checked");
    }
    if (!dllField.empty())
    {
        // a BPS patch of the module (romfs_ext, Bps.h): the game's own file is not copied into the mod
        const Bytes original = oras.Read("DllField.cro"), bps = BpsCreate(original, dllField);
        if (BpsApply(original, bps) != dllField) throw FormatError("DllField.cro: the patch does not rebuild the module");
        std::filesystem::create_directories(root / "romfs_ext");
        WriteFile((root / "romfs_ext" / "DllField.cro.bps").string(), bps);
        log.push_back("romfs_ext/DllField.cro.bps: " + std::to_string(bps.size()) + " bytes, checked");
    }
    return log;
}

}
