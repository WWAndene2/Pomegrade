#include "MapHeaders.h"

#include <set>

namespace remake
{

constexpr size_t HeaderSize = 24;

std::vector<MapHeader> FindMapHeaders(const Bytes& arm9, size_t areas, size_t matrices, size_t events, size_t* at)
{
    auto valid = [&](size_t o) {
        return o + HeaderSize <= arm9.size() && U8(arm9, o) < areas && U16(arm9, o + 2) < matrices && U16(arm9, o + 0x10) < events;
    };
    // the run naming the most different event files: zero-filled memory is "valid" too, but names one
    size_t best = 0, bestDistinct = 0, bestAt = 0;
    for (size_t start = 0; start + HeaderSize <= arm9.size(); start += 4) // an ARM table: word-aligned
    {
        if (!valid(start) || (start >= HeaderSize && valid(start - HeaderSize))) continue; // not a run's start
        size_t n = 0;
        std::set<uint16_t> eventFiles;
        for (; valid(start + n * HeaderSize); n++) eventFiles.insert(U16(arm9, start + n * HeaderSize + 0x10));
        if (eventFiles.size() > bestDistinct) { bestDistinct = eventFiles.size(); best = n; bestAt = start; }
    }
    if (bestDistinct < 100) throw FormatError("map headers: no table found in the ARM9 binary");
    std::vector<MapHeader> out(best);
    for (size_t i = 0; i < best; i++)
    {
        const size_t o = bestAt + i * HeaderSize;
        out[i].Area = U8(arm9, o);
        out[i].Matrix = U16(arm9, o + 2);
        out[i].Events = U16(arm9, o + 0x10);
        out[i].Weather = U8(arm9, o + 0x14);
        out[i].Camera = U8(arm9, o + 0x15);
    }
    if (at) *at = bestAt;
    return out;
}

}
