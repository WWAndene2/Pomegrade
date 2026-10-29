// X/Y's footsteps (OrasXyFootsteps.h): the two hooks, the zone read pc-relatively through DllField's word holding its
// global, the ground bound 13 or 17, and the table reached pc-relatively, word for word the file's
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

int main()
{
    Bytes file(4 + 17 * 9 * 8);
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
        check(at[0x102E5E90].Expected == 0xE356000D && Target(0x102E5E90, at[0x102E5E90].Value) == start && (at[0x102E5E90].Value >> 24) == 0xEB,
              "the bound's hook: bl the cave over `cmp r6, #0xD`" + where);
        check(at[0x102E5EE8].Expected == 0xE0862186 && (at[0x102E5EE8].Value >> 24) == 0xEB, "the table's hook: bl over `add r2, r6, r6, lsl #3`" + where);
        uint32_t a = start;
        check(Chain(at, a) == 0x10263D84, "the bound: the zone through DllField's word holding the global" + where);
        check(at[a].Value == 0xE5900000 && at[a + 4].Value == 0xE590001C && at[a + 8].Value == 0xE6FF0070 &&
              at[a + 12].Value == (0xE3001000 | 538) && at[a + 16].Value == 0xE1500001,
              "the bound: global, its + 0x1C, uxth, the first zone in r1, cmp" + where);
        const uint32_t bhs = a + 20;
        check((at[bhs].Value >> 24) == 0x2A && at[bhs + 4].Value == 0xE356000D && at[bhs + 8].Value == 0xE12FFF1E &&
              Target(bhs, at[bhs].Value) == bhs + 12 && at[bhs + 12].Value == 0xE3560011 && at[bhs + 16].Value == 0xE12FFF1E,
              "the bound: 13 below the first zone, 17 from it, the flags returned" + where);
        const uint32_t tableCode = Target(0x102E5EE8, at[0x102E5EE8].Value);
        check(tableCode == bhs + 20, "the table's code after the bound's" + where);
        a = tableCode;
        check(Chain(at, a) == 0x10263D84, "the table: the zone the same way" + where);
        check(at[a].Value == 0xE59CC000 && at[a + 4].Value == 0xE59CC01C && at[a + 8].Value == 0xE6FFC07C && at[a + 16].Value == 0xE15C0002,
              "the table: r12 the zone, r2 the first zone (r0 the foot kept)" + where);
        const uint32_t blo = a + 20, adr = a + 24;
        check((at[blo].Value >> 24) == 0x3A && Target(blo, at[blo].Value) == adr + 4, "the table: below the first zone, the game's" + where);
        check((at[adr].Value & 0xFFFFFF00) == 0xE28F1F00, "the table: add r1, pc, #..." + where);
        const uint32_t table = adr + 8 + 4 * (at[adr].Value & 0xFF);
        check(at[adr + 4].Value == 0xE0862186 && at[adr + 8].Value == 0xE12FFF1E && table == adr + 12, "the game's add, back, then the table" + where);
        bool same = true;
        for (size_t i = 4; i < file.size(); i += 4) same = same && at.count(table + (uint32_t)i - 4) && at[table + (uint32_t)i - 4].Value == U32(file, i);
        check(same, "the table word for word the file's" + where);
    }
    bool refused = false;
    try { XyFootstepsFieldPatchAt(0x10333000, Bytes(8)); } catch (const FormatError&) { refused = true; }
    check(refused, "a file of another size refused");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
