// X/Y's move codes (OrasXyMoveCodes.h): X's numbers made Omega Ruby's, the move-code table's loads and bounds through the
// cave, the builder's end making the seven new codes' objects, the room's table
#include "OrasXyMoveCodes.h"
#include <cstdio>
#include <map>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t B(uint32_t cond, uint32_t from, uint32_t to) { return cond << 28 | 0x0A000000 | (((to - from - 8) >> 2) & 0xFFFFFF); }
static uint32_t Target(uint32_t from, uint32_t b) { int32_t o = (int32_t)(b << 8) >> 6; return from + 8 + (uint32_t)o; }

int main()
{
    // the numbers: the same classes kept, X's seven X-only ones after Omega Ruby's 0x3E, X's spins Omega Ruby's
    bool same = true;
    for (int c = 0; c < 0x38; c++) same = same && XyMoveCodeToOras(c) == c;
    check(same, "X's 0x00-0x37 kept");
    check(XyMoveCodeToOras(0x38) == 0x3F && XyMoveCodeToOras(0x3A) == 0x41 && XyMoveCodeToOras(0x3E) == 0x45, "X's 0x38-0x3E as 0x3F-0x45");
    check(XyMoveCodeToOras(0x3F) == 0x38 && XyMoveCodeToOras(0x40) == 0x39, "X's spins as Omega Ruby's 0x38, 0x39");
    bool threw = false;
    try { XyMoveCodeToOras(0x41); } catch (const FormatError&) { threw = true; }
    check(threw, "past X's 0x40 refused");

    const uint32_t cave = 0x10340002;
    const std::vector<CodeWord> f = XyMoveCodesFieldPatchAt(cave);
    std::map<uint32_t, CodeWord> at;
    for (const CodeWord& w : f) at[w.Address] = w;
    check(at.size() == f.size(), "each word placed once");
    // the hooks: each over the instruction the game has there
    const struct { uint32_t Address, Expected; } hooks[] = {{0x1028622C, 0xE59F7C18}, {0x10286F20, 0xE59F505C}, {0x10286FC0, 0xE59F4020},
                                                             {0x10286E44, 0xE8BD81F0}};
    bool expected = true;
    for (const auto& h : hooks) expected = expected && at.count(h.Address) && at[h.Address].Expected == h.Expected;
    check(expected, "the three table loads and the builder's end over the game's words");
    check(at.count(0x10286F48) && at[0x10286F48].Expected == 0xE354003F && at[0x10286F48].Value == 0xE3540046, "the teardown's bound 0x3f -> 0x46");
    check(at.count(0x10286FC4) && at[0x10286FC4].Expected == 0xE350003F && at[0x10286FC4].Value == 0xE3500046, "the getter's bound 0x3f -> 0x46");
    // each load: to the cave, the .code word read pc-relative, the table through it into the same register, back after it
    bool loads = true;
    for (const auto& [from, reg] : std::map<uint32_t, uint32_t>{{0x1028622C, 7}, {0x10286F20, 5}, {0x10286FC0, 4}})
    {
        const uint32_t c = Target(from, at[from].Value);
        const uint32_t ldr = at[c].Value, lit = c + 8 + (ldr & 0xFFF);
        loads = loads && (ldr & 0xFFFFF000) == (0xE59F0000 | reg << 12) && at.count(lit) && at[lit].Value == XyMoveCodesPointer &&
                at[c + 4].Value == (0xE5900000 | reg << 16 | reg << 12) && Target(c + 8, at[c + 8].Value) == from + 4;
    }
    check(loads, "the loads read the table from the .code word and go back");
    // the builder's end: its literal the .code word, its loop over 0x3f-0x45, its last the builder's pop
    const uint32_t e = Target(0x10286E44, at[0x10286E44].Value);
    const uint32_t lit = e + 8 + (at[e].Value & 0xFFF);
    check(at.count(lit) && at[lit].Value == XyMoveCodesPointer, "the builder's end reads the .code word");
    check(at[e + 8].Value == 0xE3A0803F && at[e + 0x48].Value == 0xE3580046 && Target(e + 0x4C, at[e + 0x4C].Value) == e + 0x0C, "its loop: 0x3f to 0x45");
    check(Target(e + 0x14, at[e + 0x14].Value) == 0x102421F8, "each object from the builder's allocation");
    check(at[e + 0x50].Value == 0xE8BD81F0, "then the builder's pop");

    // the room: the table aligned, 70 slots and the seven vtables, zero; the .code word naming it
    const XyMoveCodesRoom r = XyMoveCodesRoomAt(0x6B0002);
    check(r.Table == 0x6B0004 && r.Room.size() == 2 + 4 * (XyMoveCodeSlots + XyMoveCodesAdded), "the room's table aligned, its size");
    check(r.Code.size() == 1 && r.Code[0].Address == XyMoveCodesPointer && r.Code[0].Value == r.Table, "the .code word names it");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
