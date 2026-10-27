// X/Y's player scale (OrasXyPlayerScale.h): the hook over the camera area search's second instruction, the cave reaching
// DllField's code word pc-relatively, the zone bounds, the path to the model and the six scale stores
#include "OrasXyPlayerScale.h"
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

int main()
{
    for (const uint32_t cave : {0x10333000u, 0x10340002u, 0x10352FF0u})
    {
        const std::vector<CodeWord> f = XyPlayerScaleFieldPatchAt(cave, 538, 556);
        std::map<uint32_t, CodeWord> at;
        for (const CodeWord& w : f) at[w.Address] = w;
        const std::string where = " (cave " + std::to_string(cave) + ")";
        check(at.size() == f.size(), "each word placed once" + where);
        const uint32_t start = (cave + 3) & ~3u;
        check(at.count(0x102CE4BC) && at[0x102CE4BC].Expected == 0xE3E02000 && Target(0x102CE4BC, at[0x102CE4BC].Value) == start &&
              (at[0x102CE4BC].Value & 0x0F000000) == 0x0B000000, "the hook: bl the cave over `mvn r2, #0`" + where);
        // the pc-relative chain: sub r1, pc, #a; sub r1, r1, #b... gives DllField's code word
        uint32_t a = start + 4, r1 = 0;
        check(at[start].Value == 0xE92D400F, "the cave saves r0-r3, lr" + where);
        for (; (at[a].Value & 0xFFFFF000) == 0xE24F1000 || (at[a].Value & 0xFFFFF000) == 0xE2411000; a += 4)
            r1 = ((at[a].Value & 0xFFFFF000) == 0xE24F1000 ? a + 8 : r1) - Immediate(at[a].Value);
        check(r1 == 0x10263D84, "the cave reads DllField's word holding the global" + where);
        bool stores = true;
        int found = 0;
        uint32_t end = 0;
        for (auto& [address, w] : at)
        {
            if ((w.Value & 0xFFFFF000) == 0xE5813000)
            {
                const uint32_t o = w.Value & 0xFFF;
                stores = stores && (o == 0x168 || o == 0x16C || o == 0x170 || o == 0x19C || o == 0x1A0 || o == 0x1A4);
                found++;
            }
            if (w.Value == 0xE8BD400F) end = address;
        }
        check(stores && found == 6, "six stores, the scale and its copy" + where);
        check(end && at[end + 4].Value == 0xE3E02000 && at[end + 8].Value == 0xE12FFF1E, "the end: the game's instruction, then back" + where);
        bool branches = true;
        for (auto& [address, w] : at)
            if (address != 0x102CE4BC && (w.Value & 0x0F000000) == 0x0A000000)
                branches = branches && Target(address, w.Value) > address && Target(address, w.Value) <= end;
        check(branches, "every branch of the cave forward, inside it" + where);
        bool bounds = false, scale = false;
        for (auto& [address, w] : at)
        {
            bounds = bounds || w.Value == (0xE3003000 | (538 & 0xFFF)) || w.Value == (0xE3003000 | (556 & 0xFFF));
            scale = scale || w.Value == (0xE3403000 | (XyPlayerScaleBits >> 28 & 0xF) << 16 | (XyPlayerScaleBits >> 16 & 0xFFF));
        }
        check(bounds && scale, "the zone bounds and 1.08 in the cave" + where);
    }
    bool threw = false;
    try { XyPlayerScaleFieldPatchAt(0x10333000, 556, 538); } catch (const FormatError&) { threw = true; }
    check(threw, "an empty zone range refused");
    printf("%s\n", ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
