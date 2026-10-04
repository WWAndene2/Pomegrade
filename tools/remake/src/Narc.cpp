#include "Narc.h"

namespace remake
{

bool Narc::Is(const Bytes& data)
{
    return data.size() >= 16 && Text(data, 0, 4) == "NARC";
}

Narc::Narc(const Bytes& data)
{
    if (!Is(data)) throw FormatError("not a NARC");
    const uint16_t headerSize = U16(data, 12);
    // sections in order: BTAF, BTNF, GMIF, each "magic, size (including its header)"
    size_t at = headerSize;
    if (Text(data, at, 4) != "BTAF") throw FormatError("NARC: no BTAF section");
    const uint32_t btafSize = U32(data, at + 4);
    const uint16_t count = U16(data, at + 8);
    const size_t table = at + 12;
    at += btafSize;
    if (Text(data, at, 4) != "BTNF") throw FormatError("NARC: no BTNF section");
    const size_t btnf = at + 8;
    at += U32(data, at + 4);
    if (Text(data, at, 4) != "GMIF") throw FormatError("NARC: no GMIF section");
    const size_t gmif = at + 8;

    Names.assign(count, "");
    // a name table whose root lists names (sub-table offset beyond its one
    // 8-byte entry): flat names in member order; nested directories are rare
    // in game archives and are not read
    if (U32(data, btnf) >= 8)
    {
        size_t n = btnf + U32(data, btnf);
        for (uint16_t i = 0; i < count; i++)
        {
            const uint8_t kind = U8(data, n++);
            if (kind == 0 || (kind & 0x80)) break;
            Names[i] = Text(data, n, kind);
            n += kind;
        }
    }
    for (uint16_t i = 0; i < count; i++)
    {
        const uint32_t start = U32(data, table + i * 8), end = U32(data, table + i * 8 + 4);
        if (end < start) throw FormatError("NARC: member " + std::to_string(i) + " ends before it starts");
        Members.push_back(Slice(data, gmif + start, end - start));
    }
}

}
