#include "OrasXyPlayerScale.h"

#include "Bytes.h"

#include <string>

namespace remake
{

namespace
{
// Omega Ruby's DllField (linked)
constexpr uint32_t Hook = 0x102CE4BC, HookWord = 0xE3E02000; // mvn r2, #0, the camera area search's second (every frame)
constexpr uint32_t GlobalWord = 0x10263D84;                   // the code's word holding the global 0x10383E40 (relocated)

uint32_t Branch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

uint32_t Movw(uint32_t reg, uint32_t value) { return 0xE3000000 | (value >> 12 & 0xF) << 16 | reg << 12 | (value & 0xFFF); }
uint32_t Movt(uint32_t reg, uint32_t value) { return 0xE3400000 | (value >> 12 & 0xF) << 16 | reg << 12 | (value & 0xFFF); }
}

std::vector<CodeWord> XyPlayerScaleFieldPatchAt(uint32_t cave, int firstZone, int lastZone)
{
    if (firstZone < 0 || lastZone < firstZone || lastZone > 0xFFFF) throw FormatError("--xy-player-scale: zones " + std::to_string(firstZone) + ":" + std::to_string(lastZone));
    cave = (cave + 3) & ~3u;
    std::vector<uint32_t> c;
    c.push_back(0xE92D400F); // push {r0, r1, r2, r3, lr}
    // r1 = the code word's address, pc-relative (the cave and the word are both in DllField's code): pc - the distance,
    // in 8-bit chunks at even shifts (sub r1, pc, #...; sub r1, r1, #...)
    uint32_t distance = cave + 4 + 8 - GlobalWord;
    for (bool first = true; distance || first; first = false)
    {
        int shift = 0;
        while (shift < 24 && !(distance & (3u << shift))) shift += 2;
        const uint32_t chunk = distance & (0xFFu << shift);
        distance -= chunk;
        const uint32_t rot = (32 - shift) / 2 % 16;
        c.push_back((first ? 0xE24F1000 : 0xE2411000) | rot << 8 | chunk >> shift);
    }
    const size_t fix = c.size();
    if (fix > 4) throw FormatError("--xy-player-scale: the cave is too far from DllField's code word");
    for (const uint32_t w : {0xE5911000u,  // ldr r1, [r1]            the global
                             0xE591201Cu,  // ldr r2, [r1, #0x1c]     the zone
                             0xE6FF2072u}) // uxth r2, r2
        c.push_back(w);
    // the player's model first (r1), then r0 = 1 when the zone is in [first, last]
    std::vector<size_t> toDone; // indices of the branches to the end
    for (const uint32_t load : {0xE5911018u,   // ldr r1, [r1, #0x18]
                                0xE591100Cu,   // ldr r1, [r1, #0xc]    the player
                                0xE591198Cu})  // ldr r1, [r1, #0x98c]  its model
    {
        c.push_back(load);
        c.push_back(0xE3510000); // cmp r1, #0
        toDone.push_back(c.size()); c.push_back(0x0);    // beq
    }
    c.push_back(0xE3A00000);                              // mov r0, #0
    c.push_back(Movw(3, (uint32_t)firstZone));
    c.push_back(0xE1520003);                              // cmp r2, r3
    const size_t below = c.size(); c.push_back(0x3);      // blo (r0 = 0)
    c.push_back(Movw(3, (uint32_t)lastZone));
    c.push_back(0xE1520003);                              // cmp r2, r3
    c.push_back(0x93A00001);                              // movls r0, #1
    const size_t pick = c.size();
    c[below] = Branch(0x3, cave + 4 * (uint32_t)below, cave + 4 * (uint32_t)pick, false);
    // r3 = 1.08 in X/Y's zones; elsewhere 1.0, but only where the scale is still ours (1.08), so Omega Ruby's own
    // scales are left alone
    c.push_back(Movw(3, XyPlayerScaleBits & 0xFFFF));
    c.push_back(Movt(3, XyPlayerScaleBits >> 16));        // r3 = 1.08
    c.push_back(0xE3500000);                              // cmp r0, #0
    const size_t inXy = c.size(); c.push_back(0x1);       // bne the stores
    c.push_back(0xE5912168);                              // ldr r2, [r1, #0x168]
    c.push_back(0xE1520003);                              // cmp r2, r3
    toDone.push_back(c.size()); c.push_back(0x1);         // bne the end (not ours)
    c.push_back(Movw(3, 0x0000));
    c.push_back(Movt(3, 0x3F80));                         // r3 = 1.0
    const size_t stores = c.size();
    c[inXy] = Branch(0x1, cave + 4 * (uint32_t)inXy, cave + 4 * (uint32_t)stores, false);
    for (const uint32_t at : {0x168u, 0x16Cu, 0x170u, 0x19Cu, 0x1A0u, 0x1A4u}) c.push_back(0xE5813000 | at); // str r3, [r1, #at]
    const size_t done = c.size();
    for (const uint32_t w : {0xE8BD400Fu,  // pop {r0, r1, r2, r3, lr}
                             HookWord,     // mvn r2, #0              the game's
                             0xE12FFF1Eu}) // bx lr
        c.push_back(w);
    for (const size_t i : toDone) c[i] = Branch(c[i], cave + 4 * (uint32_t)i, cave + 4 * (uint32_t)done, false);

    std::vector<CodeWord> w;
    w.push_back({Hook, HookWord, Branch(0xE, Hook, cave, true), "X/Y's player scale: the camera area search (each frame), the cave"});
    for (size_t i = 0; i < c.size(); i++) w.push_back({cave + 4 * (uint32_t)i, 0, c[i], "X/Y's player scale: the cave (x1.08 in X/Y's zones)"});
    return w;
}

}
