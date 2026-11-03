#include "OrasXyFieldDemo.h"

namespace remake
{

namespace
{
// Omega Ruby's DllField (linked)
constexpr uint32_t MadeHook = 0x102CA540, MadeWord = 0xE28D0008;  // add r0, sp, #8   after the sequence is stored at + 0x38
constexpr uint32_t EndHook = 0x102CA1D8, EndWord = 0xE5950038;    // ldr r0, [r5, #0x38]   before the sequence is ended
constexpr uint32_t MakerWord = 0x102464C4;                         // the import stub's word: DllSequence's sequence maker
constexpr uint32_t HeapMade = 0x598, HeapFreed = 0x4DC;            // the maker minus these: the heap's make and free

// r12 = (fromPc ? pc : r12) - value, in 8-bit chunks at even shifts (sub r12, pc|r12, #chunk ...)
void SubChunks(std::vector<uint32_t>& c, uint32_t value, bool fromPc)
{
    for (bool first = true; value || first; first = false)
    {
        int shift = 0;
        while (shift < 24 && !(value & (3u << shift))) shift += 2;
        const uint32_t chunk = value & (0xFFu << shift);
        value -= chunk;
        const uint32_t rot = (32 - shift) / 2 % 16;
        c.push_back((first && fromPc ? 0xE24FC000 : 0xE24CC000) | rot << 8 | chunk >> shift);
    }
}

uint32_t BranchLink(uint32_t from, uint32_t to) { return 0xEB000000 | (((to - from - 8) >> 2) & 0xFFFFFF); }

// a cave: push {r0-r3, r12, lr}; r0 = [event reg + 0x38]; r1 = 0; r12 = the maker - back; blx r12; pop; the replaced word; bx lr
void Cave(std::vector<uint32_t>& c, uint32_t at, uint32_t eventReg, uint32_t back, uint32_t replaced)
{
    c.push_back(0xE92D500F);                                   // push {r0, r1, r2, r3, r12, lr}
    c.push_back(0xE5900038 | eventReg << 16);                  // ldr r0, [event, #0x38]   the sequence
    c.push_back(0xE3A01000);                                   // mov r1, #0
    // r12 = the maker word's address, pc-relative (sub r12, pc, #...; sub r12, r12, #...), then the maker
    SubChunks(c, at + 4 * (uint32_t)c.size() + 8 - MakerWord, true);
    c.push_back(0xE59CC000);                                   // ldr r12, [r12]           the maker
    SubChunks(c, back, false);                                 // sub r12, r12, #back
    c.push_back(0xE12FFF3C);                                   // blx r12
    c.push_back(0xE8BD500F);                                   // pop {r0, r1, r2, r3, r12, lr}
    c.push_back(replaced);
    c.push_back(0xE12FFF1E);                                   // bx lr
}
}

std::vector<CodeWord> XyFieldDemoHeapPatchAt(uint32_t cave)
{
    cave = (cave + 3) & ~3u;
    std::vector<uint32_t> made, freed;
    Cave(made, cave, 4, HeapMade, MadeWord);                   // the event in r4 there
    const uint32_t freeCave = cave + 4 * (uint32_t)made.size();
    Cave(freed, freeCave, 5, HeapFreed, EndWord);              // and in r5 there
    std::vector<CodeWord> w;
    w.push_back({MadeHook, MadeWord, BranchLink(MadeHook, cave), "field demo: the sequence's heap made (as DllFieldDemo)"});
    w.push_back({EndHook, EndWord, BranchLink(EndHook, freeCave), "field demo: the sequence's heap freed (as DllFieldDemo)"});
    for (size_t i = 0; i < made.size(); i++) w.push_back({cave + 4 * (uint32_t)i, 0, made[i], "field demo: the heap-making cave"});
    for (size_t i = 0; i < freed.size(); i++) w.push_back({freeCave + 4 * (uint32_t)i, 0, freed[i], "field demo: the heap-freeing cave"});
    return w;
}

}
