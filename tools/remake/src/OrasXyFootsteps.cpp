#include "OrasXyFootsteps.h"

#include <string>

namespace remake
{

namespace
{
constexpr uint32_t Grounds = 17, Kinds = 9, OrasGrounds = 13, XKinds = 23;
// the tables in the modules' files (a CRO's address in the linked image is its file offset): X's DllField at 0x101F9000,
// Omega Ruby's at 0x10242000
constexpr size_t XTable = 0x102E7804 - 0x101F9000, OrasTable = 0x10334B10 - 0x10242000;
constexpr uint32_t KindOfX[Kinds] = {0, 0xFFFFFFFF, 1, 2, 3, 21, 3, 2, 2}; // Omega Ruby's kind -> X's (1: Omega Ruby's own)

// Omega Ruby's DllField (linked)
constexpr uint32_t BoundHook = 0x102E5E90, BoundWord = 0xE356000D; // cmp r6, #0xD
constexpr uint32_t TableHook = 0x102E5EE8, TableWord = 0xE0862186; // add r2, r6, r6, lsl #3 (after ldr r1, the table)
constexpr uint32_t GlobalWord = 0x10263D84;                         // the word holding DllField's global (OrasXyPlayerScale.h)

uint32_t Branch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

uint32_t Movw(uint32_t reg, uint32_t value) { return 0xE3000000 | (value >> 12 & 0xF) << 16 | reg << 12 | (value & 0xFFF); }

// rd = pc +- distance (the instruction at `at`): add/sub rd, pc, #...; then rd, rd, #... in 8-bit chunks at even shifts
void PcRelative(std::vector<uint32_t>& c, uint32_t cave, uint32_t reg, uint32_t target)
{
    const uint32_t at = cave + 4 * (uint32_t)c.size();
    const bool back = target < at + 8;
    uint32_t distance = back ? at + 8 - target : target - (at + 8);
    for (bool first = true; distance || first; first = false)
    {
        int shift = 0;
        while (shift < 24 && !(distance & (3u << shift))) shift += 2;
        const uint32_t chunk = distance & (0xFFu << shift);
        distance -= chunk;
        const uint32_t rot = (32 - shift) / 2 % 16;
        const uint32_t op = back ? 0xE2400000 : 0xE2800000; // sub / add
        c.push_back(op | (first ? 15u : reg) << 16 | reg << 12 | rot << 8 | chunk >> shift);
    }
}

// reg = the zone (DllField's global + 0x1C), then cmp it with `first` (scratch: reg and tmp)
void ZoneAtLeast(std::vector<uint32_t>& c, uint32_t cave, uint32_t reg, uint32_t tmp, uint32_t first)
{
    PcRelative(c, cave, reg, GlobalWord);
    c.push_back(0xE5900000 | reg << 16 | reg << 12);          // ldr reg, [reg]          the global
    c.push_back(0xE590001C | reg << 16 | reg << 12);          // ldr reg, [reg, #0x1C]   the zone
    c.push_back(0xE6FF0070 | reg << 12 | reg);                // uxth reg, reg
    c.push_back(Movw(tmp, first));
    c.push_back(0xE1500000 | reg << 16 | tmp);                // cmp reg, tmp
}
}

Bytes XyFootstepsFile(const Bytes& xDllField, const Bytes& orasDllField, const XySounds& sounds, int firstZone, int lastZone)
{
    if (firstZone < 0 || lastZone < firstZone || lastZone > 0xFFFF) throw FormatError("footsteps: zones " + std::to_string(firstZone) + ":" + std::to_string(lastZone));
    if (xDllField.size() < XTable + Grounds * XKinds * 8 || orasDllField.size() < OrasTable + OrasGrounds * Kinds * 8)
        throw FormatError("footsteps: a DllField too short for its table");
    // the tables where they were read (29 October): walking on ground 2, both games' SEQ_SE_FT id 0xA0023
    if (U32(xDllField, XTable + 2 * XKinds * 8) != 0xA0023 || U32(orasDllField, OrasTable + 2 * Kinds * 8) != 0xA0023)
        throw FormatError("footsteps: no footstep table where it was read (another version of the game?)");
    Bytes out(4 + Grounds * Kinds * 8, 0);
    out[0] = (uint8_t)firstZone; out[1] = (uint8_t)(firstZone >> 8);
    out[2] = (uint8_t)lastZone; out[3] = (uint8_t)(lastZone >> 8);
    for (uint32_t g = 0; g < Grounds; g++)
        for (uint32_t k = 0; k < Kinds; k++)
            for (uint32_t f = 0; f < 2; f++)
            {
                uint32_t id = 0;
                if (KindOfX[k] != 0xFFFFFFFF) id = U32(xDllField, XTable + g * XKinds * 8 + KindOfX[k] * 8 + f * 4);
                else if (g < OrasGrounds) id = U32(orasDllField, OrasTable + g * Kinds * 8 + k * 8 + f * 4);
                if (id && KindOfX[k] != 0xFFFFFFFF) id = sounds.Translate(id);
                const size_t at = 4 + (g * Kinds + k) * 8 + f * 4;
                for (int b = 0; b < 4; b++) out[at + b] = (uint8_t)(id >> (8 * b));
            }
    return out;
}

std::vector<CodeWord> XyFootstepsFieldPatchAt(uint32_t cave, const Bytes& file)
{
    if (file.size() != 4 + Grounds * Kinds * 8) throw FormatError("--xy-footsteps: not a footsteps file (oras-xy-world's xy_footsteps.bin)");
    const uint32_t first = U16(file, 0);
    cave = (cave + 3) & ~3u;
    std::vector<uint32_t> c;
    // the bound (r0 and r1 free there; flags returned): cmp r6, #17 in X/Y's zones, #13 elsewhere
    ZoneAtLeast(c, cave, 0, 1, first);
    const size_t toKalos = c.size(); c.push_back(0x2);  // bhs
    c.push_back(0xE356000D);                            // cmp r6, #13
    c.push_back(0xE12FFF1E);                            // bx lr
    c[toKalos] = Branch(0x2, cave + 4 * (uint32_t)toKalos, cave + 4 * (uint32_t)c.size(), false);
    c.push_back(0xE3560011);                            // cmp r6, #17
    c.push_back(0xE12FFF1E);                            // bx lr
    // the table (r0, the foot, kept; r2 and r12 free): r1 the X/Y table in X/Y's zones, then the game's add
    const uint32_t tableCode = cave + 4 * (uint32_t)c.size();
    ZoneAtLeast(c, cave, 12, 2, first);
    const size_t keep = c.size(); c.push_back(0x3);     // blo the game's
    const size_t adr = c.size(); c.push_back(0);        // add r1, pc, #(the table)
    const size_t game = c.size();
    c[keep] = Branch(0x3, cave + 4 * (uint32_t)keep, cave + 4 * (uint32_t)game, false);
    c.push_back(TableWord);                             // add r2, r6, r6, lsl #3
    c.push_back(0xE12FFF1E);                            // bx lr
    const uint32_t table = cave + 4 * (uint32_t)c.size();
    const uint32_t distance = table - (cave + 4 * (uint32_t)adr + 8);
    if (distance > 0xFF * 4 || distance % 4) throw FormatError("--xy-footsteps: the table out of the cave's reach");
    c[adr] = 0xE28F1F00 | distance >> 2;                // add r1, pc, #distance (rotated by 30: x4)
    for (size_t at = 4; at < file.size(); at += 4) c.push_back(U32(file, at));

    std::vector<CodeWord> w;
    w.push_back({BoundHook, BoundWord, Branch(0xE, BoundHook, cave, true), "X/Y's footsteps: the ground bound, 17 in X/Y's zones (the cave)"});
    w.push_back({TableHook, TableWord, Branch(0xE, TableHook, tableCode, true), "X/Y's footsteps: X/Y's table in X/Y's zones (the cave)"});
    for (size_t i = 0; i < c.size(); i++) w.push_back({cave + 4 * (uint32_t)i, 0, c[i], "X/Y's footsteps: the cave and its table"});
    return w;
}

}
