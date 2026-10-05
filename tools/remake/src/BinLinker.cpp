#include "BinLinker.h"

namespace remake
{

BinLinker BinLinker::Read(const Bytes& data, const std::string& tag)
{
    if (tag.size() != 2) throw FormatError("container tag must be 2 letters");
    if (Text(data, 0, 2) != tag) throw FormatError("not a " + tag + " container");
    BinLinker b;
    b.Tag = tag;
    const uint16_t count = U16(data, 2);
    uint32_t start = U32(data, 4);
    // the largest power of two (4..0x80) dividing every offset
    b.Align = 0x80;
    for (uint32_t i = 0; i <= count; i++)
        while (U32(data, 4 + i * 4) % b.Align) b.Align /= 2;
    if (b.Align < 4) throw FormatError(tag + ": an offset not 4-byte aligned");
    for (uint16_t i = 0; i < count; i++)
    {
        const uint32_t end = U32(data, 8 + i * 4);
        if (end < start) throw FormatError(tag + ": file " + std::to_string(i) + " ends before it starts");
        b.Files.push_back(Slice(data, start, end - start));
        start = end;
    }
    if (start != data.size()) throw FormatError(tag + ": its files end at " + std::to_string(start) + ", the container at " + std::to_string(data.size()));
    return b;
}

Bytes BinLinker::Write() const
{
    Bytes out = {(uint8_t)Tag[0], (uint8_t)Tag[1], (uint8_t)(Files.size() & 0xFF), (uint8_t)(Files.size() >> 8)};
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((v >> (8 * i)) & 0xFF); };
    const auto aligned = [&](size_t v) { return (uint32_t)((v + Align - 1) / Align * Align); };
    uint32_t at = aligned(4 + 4 * (Files.size() + 1));
    for (const Bytes& f : Files) { put32(at); at += aligned(f.size()); }
    put32(at);
    out.resize(aligned(out.size()), 0);
    for (const Bytes& f : Files)
    {
        out.insert(out.end(), f.begin(), f.end());
        out.resize(aligned(out.size()), 0);
    }
    return out;
}

}
