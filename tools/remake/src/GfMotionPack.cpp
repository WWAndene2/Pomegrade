#include "GfMotionPack.h"

#include <string>
#include <vector>

namespace remake
{

namespace
{

constexpr uint32_t XySlots = 173, OrasSlots = 239;

// the slot's bytes: up to the next non-empty slot's start, or the pack's end
Bytes SlotBytes(const Bytes& pack, uint32_t count, uint32_t k)
{
    const uint32_t from = U32(pack, 4 + 4 * k);
    uint32_t to = U32(pack, 4 + 4 * count);
    for (uint32_t j = k + 1; j < count; j++) if (U32(pack, 4 + 4 * j)) { to = U32(pack, 4 + 4 * j); break; }
    return Bytes(pack.begin() + from, pack.begin() + to);
}

// X/Y slots no ORAS motion matches, and the ORAS slots none of ORAS's characters fills (its 326 packs of 239 slots,
// read 17 October: 113) less the 12 the moves above fill (95-98, 122, 123, 131-136), the first 75 of the 101 left: the
// n-th of the first list goes to the n-th of the second (GfMotionPack.h)
constexpr uint8_t XyOwn[] = {14, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
    69, 70, 71, 91, 92, 93, 94, 95, 96, 98, 99, 139, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151};
constexpr uint8_t OrasFree[] = {28, 31, 32, 33, 34, 35, 36, 37, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 105, 106, 107, 108,
    109, 161, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210};
static_assert(sizeof XyOwn == sizeof OrasFree, "one free ORAS slot for each X/Y motion of its own");

}

int XyMotionSlotInOras(uint32_t xy, bool* own)
{
    if (own) *own = false;
    if (xy <= 3) return (int)xy;
    if (xy >= 4 && xy <= 6) return (int)xy + 1;
    if (xy >= 7 && xy <= 9) return (int)xy + 5;
    if (xy == 10) return 16;
    if ((xy >= 11 && xy <= 13) || xy == 15 || xy == 16) return (int)xy + 14;
    if (xy == 26) return 40;
    if (xy >= 72 && xy <= 90) return (int)xy + 14;
    if (xy == 97) return 178;
    if (xy >= 100 && xy <= 138) return (int)xy + 10;
    if (xy == 140) return 156;
    if (xy == 172) return 159;
    for (size_t i = 0; i < sizeof XyOwn; i++)
        if (XyOwn[i] == xy) { if (own) *own = true; return OrasFree[i]; }
    return -1;
}

bool IsGfMotionPack(const Bytes& data, uint32_t slots)
{
    if (data.size() < 4 + 4 * ((size_t)slots + 1) || U32(data, 0) != slots) return false;
    uint32_t last = 4 + 4 * (slots + 1);
    for (uint32_t k = 0; k <= slots; k++)
    {
        const uint32_t at = U32(data, 4 + 4 * k);
        if (!at) continue;
        if (at < last || at > data.size()) return false;
        last = at;
    }
    return U32(data, 4 + 4 * slots) != 0;
}

Bytes XyMotionPackToOras(const Bytes& pack)
{
    if (!IsGfMotionPack(pack, XySlots)) throw FormatError("not an X/Y motion pack of 173 slots");
    std::vector<Bytes> slots(OrasSlots);
    for (uint32_t k = 0; k < XySlots; k++)
        if (U32(pack, 4 + 4 * k))
        {
            const int to = XyMotionSlotInOras(k);
            if (to < 0) throw FormatError("X/Y motion slot " + std::to_string(k) + " has no ORAS slot (GfMotionPack.h)");
            if (!slots[(size_t)to].empty()) throw FormatError("X/Y motion slots " + std::to_string(k) + " and another both go to ORAS slot " + std::to_string(to));
            slots[(size_t)to] = SlotBytes(pack, XySlots, k);
        }
    Bytes out(4 + 4 * (OrasSlots + 1), 0);
    auto put = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) out[at + i] = (uint8_t)(v >> (8 * i)); };
    put(0, OrasSlots);
    for (uint32_t k = 0; k < OrasSlots; k++)
    {
        if (slots[k].empty()) continue;
        while (out.size() % 4) out.push_back(0); // a motion's alignments count from its start (GfMotion.h)
        put(4 + 4 * k, (uint32_t)out.size());
        out.insert(out.end(), slots[k].begin(), slots[k].end());
    }
    put(4 + 4 * OrasSlots, (uint32_t)out.size());
    return out;
}

}
