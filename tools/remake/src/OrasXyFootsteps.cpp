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

void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b.at(at + i) = (uint8_t)(v >> (8 * i)); }

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

Bytes XyFootstepsFile(const Bytes& xDllField, const Bytes& orasDllField, XySounds& sounds, int firstZone, int lastZone)
{
    if (firstZone < 0 || lastZone < firstZone || lastZone > 0xFFFF) throw FormatError("footsteps: zones " + std::to_string(firstZone) + ":" + std::to_string(lastZone));
    if (xDllField.size() < XTable + Grounds * XKinds * 8 || orasDllField.size() < OrasTable + OrasGrounds * Kinds * 8)
        throw FormatError("footsteps: a DllField too short for its table");
    // the tables where they were read (29 October): walking on ground 2, both games' SEQ_SE_FT id 0xA0023
    if (U32(xDllField, XTable + 2 * XKinds * 8) != 0xA0023 || U32(orasDllField, OrasTable + 2 * Kinds * 8) != 0xA0023)
        throw FormatError("footsteps: no footstep table where it was read (another version of the game?)");
    // a ground's row (9 kinds x 2 feet) of Omega Ruby's table, or of X's made Omega Ruby's (its kinds, its ids translated)
    const auto orasRow = [&](uint32_t g) {
        std::vector<uint32_t> row(Kinds * 2, 0);
        if (g < OrasGrounds)
            for (uint32_t i = 0; i < Kinds * 2; i++) row[i] = U32(orasDllField, OrasTable + g * Kinds * 8 + i * 4);
        return row;
    };
    const auto xRow = [&](uint32_t g) {
        std::vector<uint32_t> row(Kinds * 2, 0);
        for (uint32_t k = 0; k < Kinds; k++)
            for (uint32_t f = 0; f < 2; f++)
            {
                uint32_t id = 0;
                if (KindOfX[k] != 0xFFFFFFFF) id = U32(xDllField, XTable + g * XKinds * 8 + KindOfX[k] * 8 + f * 4);
                else if (g < OrasGrounds) id = U32(orasDllField, OrasTable + g * Kinds * 8 + k * 8 + f * 4);
                if (id && KindOfX[k] != 0xFFFFFFFF) id = sounds.Translate(id);
                row[k * 2 + f] = id;
            }
        return row;
    };
    // the surfaces (OrasXyFootsteps.h): which game's ground each plays, at what volume. Volumes chosen from the samples'
    // loudness (prototype/footstep_sounds.py, 30 October: grass FT_04 -16.9 dB RMS, stone FT_03 -20.0, wood FT_06 -17.3,
    // X's carpet FT_11 -17.8 and its centroid the lowest of the dry steps, 1.6 kHz, X's leaves FT_06 -22.9)
    struct Surface { int Ground; bool FromX; uint32_t Base; int Percent; };
    static const Surface surfaces[] = {
        {SoftGrass, false, 4, 50}, {Dirt, true, 11, 70}, {Paved, false, 3, 75}, {Wood, false, 6, 75},
        {ForestFloor, true, 6, 100}, {CaveRock, false, 3, 100}, {Carpet, true, 11, 45}, {IndoorFloor, false, 3, 50},
    };
    std::vector<std::vector<uint32_t>> extra(FootstepGrounds);
    for (const Surface& s : surfaces)
    {
        std::vector<uint32_t> row = s.FromX ? xRow(s.Base) : orasRow(s.Base);
        // the steps quieter (kinds 0-2: walking, Omega Ruby's own kind 1, running), the jumps and a ledge's landing as loud
        // as on the base ground (kinds 3-8: SEQ_SE_RS_JUMP, SEQ_SE_FLD_DANSA, SEQ_SE_FLD_CHAKUCHI there)
        if (s.Percent != 100)
            for (uint32_t i = 0; i < 3 * 2; i++) if (row[i]) row[i] = sounds.QuietCopy(row[i], s.Percent);
        extra[(size_t)s.Ground] = row;
    }

    Bytes out(4 + 2 * FootstepGrounds * Kinds * 8, 0);
    out[0] = (uint8_t)firstZone; out[1] = (uint8_t)(firstZone >> 8);
    out[2] = (uint8_t)lastZone; out[3] = (uint8_t)(lastZone >> 8);
    for (int table = 0; table < 2; table++) // Hoenn's, then Kalos's
        for (uint32_t g = 0; g < FootstepGrounds; g++)
        {
            const std::vector<uint32_t> row = g >= (uint32_t)SoftGrass ? extra[g] : table == 0 ? orasRow(g) : g < Grounds ? xRow(g) : std::vector<uint32_t>(Kinds * 2, 0);
            for (uint32_t i = 0; i < Kinds * 2; i++)
                Put32(out, 4 + ((size_t)table * FootstepGrounds * Kinds * 2 + g * Kinds * 2 + i) * 4, row[i]);
        }
    return out;
}

std::vector<CodeWord> XyFootstepsFieldPatchAt(uint32_t cave, const Bytes& file)
{
    const size_t tableBytes = FootstepGrounds * Kinds * 8;
    if (file.size() != 4 + 2 * tableBytes) throw FormatError("--xy-footsteps: not a footsteps file (oras-xy-world's xy_footsteps.bin)");
    const uint32_t first = U16(file, 0);
    cave = (cave + 3) & ~3u;
    // the two tables first, then the code reaching them backwards
    std::vector<uint32_t> c;
    for (size_t at = 4; at < file.size(); at += 4) c.push_back(U32(file, at));
    const uint32_t hoenn = cave, kalos = cave + (uint32_t)tableBytes;
    // the table (r0, the foot, kept; r2 and r12 free): r1 Kalos's in X/Y's zones, Hoenn's elsewhere, then the game's add
    const uint32_t tableCode = cave + 4 * (uint32_t)c.size();
    ZoneAtLeast(c, cave, 12, 2, first);
    const size_t toKalos = c.size(); c.push_back(0x2); // bhs
    PcRelative(c, cave, 1, hoenn);
    c.push_back(TableWord);                            // add r2, r6, r6, lsl #3
    c.push_back(0xE12FFF1E);                           // bx lr
    c[toKalos] = Branch(0x2, cave + 4 * (uint32_t)toKalos, cave + 4 * (uint32_t)c.size(), false);
    PcRelative(c, cave, 1, kalos);
    c.push_back(TableWord);                            // add r2, r6, r6, lsl #3
    c.push_back(0xE12FFF1E);                           // bx lr

    std::vector<CodeWord> w;
    w.push_back({BoundHook, BoundWord, 0xE3560000 | FootstepGrounds, "footsteps: the ground bound, 25 (the surfaces)"});
    w.push_back({TableHook, TableWord, Branch(0xE, TableHook, tableCode, true), "footsteps: Hoenn's or Kalos's table (the cave)"});
    for (size_t i = 0; i < c.size(); i++) w.push_back({cave + 4 * (uint32_t)i, 0, c[i], "footsteps: the tables and their cave"});
    return w;
}

}
