#include "Garc.h"

namespace remake
{

bool Garc::Is(const Bytes& data)
{
    return data.size() >= 0x1C && Text(data, 0, 4) == "CRAG";
}

Garc::Garc(const Bytes& data)
{
    if (!Is(data)) throw FormatError("not a GARC");
    const uint32_t headerSize = U32(data, 4);
    VersionNumber = U16(data, 0x0A);
    if (VersionNumber != 0x0400 && VersionNumber != 0x0600)
        throw FormatError("GARC: unknown version " + std::to_string(VersionNumber));
    const uint32_t dataStart = U32(data, 0x10);

    size_t at = headerSize;
    if (Text(data, at, 4) != "OTAF") throw FormatError("GARC: no FATO section");
    const uint32_t fatoSize = U32(data, at + 4);
    const uint16_t count = U16(data, at + 8);
    const size_t offsets = at + 12;
    at += fatoSize;
    if (Text(data, at, 4) != "BTAF") throw FormatError("GARC: no FATB section");
    const size_t fatb = at + 12; // after magic, size, entry count

    for (uint16_t i = 0; i < count; i++)
    {
        size_t e = fatb + U32(data, offsets + i * 4);
        const uint32_t mask = U32(data, e);
        e += 4;
        std::vector<Bytes> subs;
        for (int bit = 0; bit < 32; bit++)
        {
            if (!(mask >> bit & 1)) continue;
            subs.resize(bit + 1);
            const uint32_t start = U32(data, e), end = U32(data, e + 4);
            e += 12; // start, end, length
            if (end < start) throw FormatError("GARC: entry " + std::to_string(i) + " ends before it starts");
            subs[bit] = Slice(data, dataStart + start, end - start);
        }
        Entries.push_back(std::move(subs));
        Present.push_back(mask);
    }
}

}
