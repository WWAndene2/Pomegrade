// The footsteps (OrasXyFootsteps.h): the ground bound made 25, the table hook reading the zone pc-relatively through
// DllField's word holding its global, Hoenn's table below the first X/Y zone and Kalos's from it, both reached
// pc-relatively and word for word the file's; on a surface, the step's volume varied at random and its pitch by a pattern
#include "OrasXyFootsteps.h"
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

static uint32_t Target(uint32_t from, uint32_t b) { int32_t o = (int32_t)(b << 8) >> 6; return from + 8 + (uint32_t)o; }
static uint32_t Immediate(uint32_t w) { const uint32_t r = (w >> 8 & 0xF) * 2, v = w & 0xFF; return r ? (v >> r | v << (32 - r)) : v; }

// the address a pc-relative chain from `a` leaves in its register (add/sub rd, pc, #...; then add/sub rd, rd, #...)
static uint32_t Chain(std::map<uint32_t, CodeWord>& at, uint32_t& a)
{
    uint32_t r = 0;
    for (;; a += 4)
    {
        const uint32_t w = at[a].Value, op = w & 0xFFF00000, rn = w >> 16 & 0xF;
        if (op != 0xE2400000 && op != 0xE2800000) break;
        const uint32_t base = rn == 15 ? a + 8 : r;
        r = op == 0xE2800000 ? base + Immediate(w) : base - Immediate(w);
    }
    return r;
}

// the same for a chain into r12 padded with nops to three words (the seed's address)
static uint32_t Chain12(std::map<uint32_t, CodeWord>& at, uint32_t a)
{
    const uint32_t end = a + 12;
    const uint32_t r = Chain(at, a);
    for (; a < end; a += 4) if (at[a].Value != 0xE320F000) return 0;
    return r;
}

int main()
{
    const size_t tableBytes = 25 * 9 * 8;
    Bytes file(4 + 2 * tableBytes);
    file[0] = 538 & 0xFF; file[1] = 538 >> 8; file[2] = 541 & 0xFF; file[3] = 541 >> 8;
    for (size_t i = 4; i < file.size(); i++) file[i] = (uint8_t)(i * 7);
    for (const uint32_t cave : {0x10333000u, 0x10340002u, 0x10352FF0u})
    {
        const std::vector<CodeWord> f = XyFootstepsFieldPatchAt(cave, file);
        std::map<uint32_t, CodeWord> at;
        for (const CodeWord& w : f) at[w.Address] = w;
        const std::string where = " (cave " + std::to_string(cave) + ")";
        check(at.size() == f.size(), "each word placed once" + where);
        const uint32_t start = (cave + 3) & ~3u;
        check(at[0x102E5E90].Expected == 0xE356000D && at[0x102E5E90].Value == 0xE3560019, "the bound: `cmp r6, #0xD` made `cmp r6, #25`" + where);
        check(at[0x102E5EE8].Expected == 0xE0862186 && (at[0x102E5EE8].Value >> 24) == 0xEB, "the table's hook: bl over `add r2, r6, r6, lsl #3`" + where);
        bool same = true;
        for (size_t i = 4; i < file.size(); i += 4) same = same && at.count(start + (uint32_t)i - 4) && at[start + (uint32_t)i - 4].Value == U32(file, i);
        check(same, "the two tables word for word the file's, at the cave's start" + where);
        const uint32_t tableCode = Target(0x102E5EE8, at[0x102E5EE8].Value);
        check(tableCode == start + 2 * tableBytes, "the table's code after the tables" + where);
        // on a surface, walking or running: the foot's column a random bit of the LCRNG, its seed past the code
        check(at[tableCode].Value == 0xE3560011 && (at[tableCode + 4].Value >> 24) == 0x3A && at[tableCode + 8].Value == 0xE3550002 &&
                  (at[tableCode + 12].Value >> 24) == 0x8A && Target(tableCode + 4, at[tableCode + 4].Value) == tableCode + 144 &&
                  Target(tableCode + 12, at[tableCode + 12].Value) == tableCode + 144,
              "the random foot: ground 17 up and kinds 0-2 only" + where);
        uint32_t s = tableCode + 16;
        const uint32_t seed = Chain12(at, s);
        check(at[tableCode + 28].Value == 0xE59C2000 && at[tableCode + 40].Value == 0xE0020291 && at[tableCode + 52].Value == 0xE58C2000 &&
                  at[tableCode + 56].Value == 0xE7E71452 && at[tableCode + 96].Value == 0xEDCD7A01 && at[tableCode + 100].Value == 0xE59C1004 &&
                  at[tableCode + 108].Value == 0xE3510005 && at[tableCode + 128].Value == 0xED9C7A00 && at[tableCode + 140].Value == 0xEDCD7A02 &&
                  at.count(seed) && seed > tableCode + 144 && at[seed + 4].Value == 0 && at[seed + 8].Value == 0x3F70A3D7 &&
                  at[seed + 12].Value == 0x3F87AE14 && at[seed + 16].Value == 0x3F800000 && at[seed + 20].Value == 0x3F87AE14 &&
                  at[seed + 24].Value == 0x3F70A3D7,
              "the variation: the volume (sp + 4) by the LCRNG, the pitch (sp + 8) by the pattern 0.94 1.06 1 1.06 0.94" + where);
        uint32_t a = tableCode + 144;
        check(Chain(at, a) == 0x10263D84, "the table: the zone through DllField's word holding the global" + where);
        check(at[a].Value == 0xE59CC000 && at[a + 4].Value == 0xE59CC01C && at[a + 8].Value == 0xE6FFC07C && at[a + 12].Value == (0xE3002000 | 538) &&
              at[a + 16].Value == 0xE15C0002,
              "the table: r12 the zone, r2 the first zone (r0 the foot kept), cmp" + where);
        const uint32_t bhs = a + 20;
        check((at[bhs].Value >> 24) == 0x2A, "the table: from the first zone, Kalos's" + where);
        a = bhs + 4;
        check(Chain(at, a) == start, "below it: r1 Hoenn's table" + where);
        check(at[a].Value == 0xE0862186 && at[a + 4].Value == 0xE12FFF1E, "then the game's add, back" + where);
        a = Target(bhs, at[bhs].Value);
        check(a > bhs, "the branch forward" + where);
        check(Chain(at, a) == start + tableBytes, "from it: r1 Kalos's table" + where);
        check(at[a].Value == 0xE0862186 && at[a + 4].Value == 0xE12FFF1E, "then the game's add, back" + where);
    }
    bool refused = false;
    try { XyFootstepsFieldPatchAt(0x10333000, Bytes(8)); } catch (const FormatError&) { refused = true; }
    check(refused, "a file of another size refused");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
