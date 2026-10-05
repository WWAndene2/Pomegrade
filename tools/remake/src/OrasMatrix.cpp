#include "OrasMatrix.h"
#include "BinLinker.h"

namespace remake
{

static void Put16(Bytes& b, uint16_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }

OrasMatrix OrasMatrix::Read(const Bytes& mm)
{
    const BinLinker c = BinLinker::Read(mm, "MM");
    if (c.Files.size() != 2) throw FormatError("a map matrix holds 2 files, this one " + std::to_string(c.Files.size()));
    const Bytes& f = c.Files[0];
    if (f.size() < 8 || U16(f, 0) != 1 || U16(f, 2) != 0) throw FormatError("map matrix: file 0 does not start 1, 0");
    OrasMatrix m;
    m.Width = U16(f, 4); m.Height = U16(f, 6);
    const size_t cells = (size_t)m.Width * m.Height, blocks = cells * BlocksPerPiece * BlocksPerPiece;
    const size_t shortSize = (8 + 2 * cells + 3) & ~(size_t)3, fullSize = 8 + 2 * (cells + blocks + cells);
    const bool full = f.size() == fullSize;
    if (!full && f.size() != shortSize) throw FormatError("map matrix: file 0 is " + std::to_string(f.size()) + " bytes, fits neither layout");
    auto words = [&](size_t at, size_t n) { std::vector<uint16_t> v(n); for (size_t k = 0; k < n; k++) v[k] = U16(f, at + 2 * k); return v; };
    m.Pieces = words(8, cells);
    if (full)
    {
        m.Zones = words(8 + 2 * cells, blocks);
        m.Third = words(8 + 2 * (cells + blocks), cells);
    }
    m.File1 = c.Files[1];
    return m;
}

Bytes OrasMatrix::Write() const
{
    const size_t cells = (size_t)Width * Height, blocks = cells * BlocksPerPiece * BlocksPerPiece;
    if (Pieces.size() != cells) throw FormatError("map matrix: the piece grid is not width x height");
    if (!Zones.empty() && (Zones.size() != blocks || Third.size() != cells)) throw FormatError("map matrix: the zone or third grid does not fit width x height");
    Bytes f;
    for (uint16_t v : {(uint16_t)1, (uint16_t)0, Width, Height}) Put16(f, v);
    for (uint16_t v : Pieces) Put16(f, v);
    if (Zones.empty()) { while (f.size() % 4) f.push_back(0); }
    else
    {
        for (uint16_t v : Zones) Put16(f, v);
        for (uint16_t v : Third) Put16(f, v); // 36 wh + 8 bytes in all: always a whole number of 4, no padding
    }
    BinLinker c;
    c.Tag = "MM";
    c.Align = 4;
    c.Files = {f, File1};
    return c.Write();
}

}
