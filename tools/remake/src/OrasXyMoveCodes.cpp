#include "OrasXyMoveCodes.h"

#include <cstdio>
#include <iterator>

namespace remake
{

namespace
{
// Omega Ruby's DllField (linked): the table's three users and the allocation the builder calls
constexpr uint32_t BuilderLoad = 0x1028622C, BuilderEnd = 0x10286E44;   // ldr r7, =table; pop {r4-r8, pc}
constexpr uint32_t TeardownLoad = 0x10286F20, TeardownBound = 0x10286F48; // ldr r5, =table; cmp r4, #0x3f
constexpr uint32_t GetterLoad = 0x10286FC0, GetterBound = 0x10286FC4;     // ldr r4, =table; cmp r0, #0x3f
constexpr uint32_t Allocate = 0x102421F8;                                 // the builder's 4-byte allocation (r0 size, r1 heap)

uint32_t Branch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}
void Put(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }
}

int XyMoveCodeToOras(int xCode)
{
    if (xCode < 0x38) return xCode;                            // the same classes (their RTTI names)
    if (xCode <= 0x3E) return OrasMoveCodes + (xCode - 0x38);  // X-only: Tsutikemuri x3, Yagi x3, TravelingPoke
    if (xCode == 0x3F) return 0x38;                            // MoveCodeSpinRight
    if (xCode == 0x40) return 0x39;                            // MoveCodeSpinLeft
    char m[64];
    snprintf(m, sizeof m, "X/Y move code 0x%X: past X/Y's 0x40", xCode);
    throw FormatError(m);
}

XyMoveCodesRoom XyMoveCodesRoomAt(uint32_t at, const std::vector<uint32_t>& vtables)
{
    if (vtables.size() > XyMoveCodesAdded) throw FormatError("X/Y's move codes: more classes than codes added");
    XyMoveCodesRoom r;
    r.Table = (at + 3) & ~3u;
    r.Room.assign(r.Table - at, 0);
    for (int k = 0; k < XyMoveCodeSlots; k++) Put(r.Room, 0);                         // the table (the builder fills it)
    for (int k = 0; k < XyMoveCodesAdded; k++) Put(r.Room, k < (int)vtables.size() ? vtables[k] : 0); // the new codes' vtables
    r.Code.push_back({XyMoveCodesPointer, 0, r.Table, "X/Y's move codes: the room table's address"});
    return r;
}

std::vector<CodeWord> XyMoveCodesFieldPatchAt(uint32_t cave)
{
    cave = (cave + 3) & ~3u;
    std::vector<CodeWord> w;
    const auto code = [&](uint32_t at, std::initializer_list<uint32_t> words, const char* why) {
        uint32_t a = at;
        for (const uint32_t v : words) { w.push_back({a, 0, v, why}); a += 4; }
        return a;
    };
    // a table load (ldr rN, =table): through the cave, the table's address from the .code word
    const auto load = [&](uint32_t from, uint32_t expected, uint32_t reg, uint32_t at, const char* why) {
        w.push_back({from, expected, Branch(0xE, from, at, false), why});
        return code(at, {0xE59F0004 | reg << 12,                // ldr rN, [pc, #4]   the .code word
                         0xE5900000 | reg << 16 | reg << 12,    // ldr rN, [rN]       the table
                         Branch(0xE, at + 8, from + 4, false),  // b back
                         XyMoveCodesPointer}, why);
    };
    uint32_t at = cave;
    at = load(BuilderLoad, 0xE59F7C18, 7, at, "X/Y's move codes: the builder's table, the room's");
    at = load(TeardownLoad, 0xE59F505C, 5, at, "X/Y's move codes: the teardown's table, the room's");
    at = load(GetterLoad, 0xE59F4020, 4, at, "X/Y's move codes: the getter's table, the room's");
    w.push_back({TeardownBound, 0xE354003F, 0xE3540000 | XyMoveCodeSlots, "X/Y's move codes: the teardown's bound, 0x46"});
    w.push_back({GetterBound, 0xE350003F, 0xE3500000 | XyMoveCodeSlots, "X/Y's move codes: the getter's bound, 0x46"});

    // the builder's end: the new codes' objects (4 bytes, their vtable), as the builder makes the others; a code's vtable
    // from the list after the table, or Omega Ruby's MoveCodeNone (slot 0's) while X's class is not ported
    const uint32_t end = at;
    w.push_back({BuilderEnd, 0xE8BD81F0, Branch(0xE, BuilderEnd, end, false), "X/Y's move codes: the builder's end, the new codes"});
    const uint32_t loop = end + 0x0C, skip = end + 0x40;
    code(end, {
        0xE59F504C,                                  // 00 ldr r5, [pc, #0x4c]   the .code word
        0xE5955000,                                  // 04 ldr r5, [r5]          the table
        0xE3A08000 | OrasMoveCodes,                  // 08 mov r8, #0x3f
        0xE1A01006,                                  // 0C mov r1, r6            the builder's heap
        0xE3A00004,                                  // 10 mov r0, #4
        Branch(0xE, end + 0x14, Allocate, true),     // 14 bl the allocation
        0xE3500000,                                  // 18 cmp r0, #0
        Branch(0x0, end + 0x1C, skip, false),        // 1C beq 40
        0xE2852000 | (4 * XyMoveCodesAdded),         // 20 add r2, r5, #0x1c     the list (after the table) - 0x3f words
        0xE7921108,                                  // 24 ldr r1, [r2, r8, lsl #2]
        0xE3510000,                                  // 28 cmp r1, #0
        0x05951000,                                  // 2C ldreq r1, [r5]        slot 0's object
        0x05911000,                                  // 30 ldreq r1, [r1]        its vtable (MoveCodeNone)
        0xE5801000,                                  // 34 str r1, [r0]
        0xE320F000,                                  // 38 nop
        0xE320F000,                                  // 3C nop
        0xE7850108,                                  // 40 str r0, [r5, r8, lsl #2]
        0xE2888001,                                  // 44 add r8, r8, #1
        0xE3580000 | XyMoveCodeSlots,                // 48 cmp r8, #0x46
        Branch(0x3, end + 0x4C, loop, false),        // 4C blo 0C
        0xE8BD81F0,                                  // 50 pop {r4, r5, r6, r7, r8, pc}
        XyMoveCodesPointer,                          // 54
    }, "X/Y's move codes: the new codes' objects (cave)");
    return w;
}

}
