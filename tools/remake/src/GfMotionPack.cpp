#include "GfMotionPack.h"

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

// the ORAS slot an X/Y slot goes to, -1 none (GfMotionPack.h)
int OrasSlot(uint32_t xy)
{
    if (xy <= 3) return (int)xy;
    if (xy >= 72 && xy <= 89) return (int)xy + 14;
    if (xy >= 114 && xy <= 138) return (int)xy + 10;
    if (xy == 26) return 40;
    if (xy == 97) return 178;
    return -1;
}

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
        if (U32(pack, 4 + 4 * k) && OrasSlot(k) >= 0) slots[(size_t)OrasSlot(k)] = SlotBytes(pack, XySlots, k);
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
