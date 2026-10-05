#include "ZoneEvents.h"

namespace remake
{

ZoneEvents ZoneEvents::Read(const Bytes& d)
{
    ZoneEvents e;
    size_t at = 0;
    auto list = [&](size_t entrySize) {
        const uint32_t n = U32(d, at);
        at += 4;
        const size_t start = at;
        if ((uint64_t)n * entrySize > d.size() - at) throw FormatError("zone events: a list runs past the file");
        at += (size_t)n * entrySize;
        return std::make_pair(start, (size_t)n);
    };
    e.BgEvents = list(0x14).second;
    const auto [objects, objectCount] = list(0x20);
    for (size_t i = 0; i < objectCount; i++)
    {
        const size_t o = objects + i * 0x20;
        ZoneObject x;
        x.Id = U16(d, o); x.Graphics = U16(d, o + 2); x.Script = U16(d, o + 0x0A);
        x.X = U16(d, o + 0x18); x.Z = U16(d, o + 0x1A);
        x.Y = (int32_t)U32(d, o + 0x1C) / 65536.0f;
        e.Objects.push_back(x);
    }
    const auto [warps, warpCount] = list(0x0C);
    for (size_t i = 0; i < warpCount; i++)
    {
        const size_t o = warps + i * 0x0C;
        e.Warps.push_back({U16(d, o), U16(d, o + 2), U16(d, o + 4), U16(d, o + 6)});
    }
    e.CoordEvents = list(0x10).second;
    if (at != d.size()) throw FormatError("zone events: " + std::to_string(d.size() - at) + " bytes after the last list");
    return e;
}

}
