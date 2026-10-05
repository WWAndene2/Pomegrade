#include "OrasZone.h"
#include "BinLinker.h"

#include <cstring>

namespace remake
{

template <typename T>
static void ReadArray(const Bytes& b, size_t& at, size_t count, size_t stride, std::vector<T>& out)
{
    for (size_t i = 0; i < count; i++)
    {
        T item;
        if (at + stride > b.size()) throw FormatError("zone: an array runs past its file");
        for (size_t k = 0; k < stride / 2; k++) item.Raw[k] = U16(b, at + k * 2);
        out.push_back(item);
        at += stride;
    }
}

OrasZone OrasZone::Read(const Bytes& zo)
{
    const BinLinker c = BinLinker::Read(zo, "ZO");
    if (c.Files.size() < 3) throw FormatError("zone: fewer than three files");
    OrasZone z;
    const Bytes& h = c.Files[0];
    if (h.size() != 56) throw FormatError("zone: header of " + std::to_string(h.size()) + " bytes, not 56");
    for (size_t i = 0; i < 28; i++) z.Header[i] = U16(h, i * 2);

    const Bytes& e = c.Files[1];
    if (e.size() < 12) throw FormatError("zone: events file too short");
    const size_t size = (size_t)U32(e, 0) + 4; // the field counts what follows it
    const size_t nf = e[4], nn = e[5], nw = e[6], nt = e[7], n5 = U32(e, 8);
    if (n5 > 4096) throw FormatError("zone: " + std::to_string(n5) + " entries of the fifth kind");
    const size_t expected = 12 + nf * 0x14 + nn * 0x30 + (nw + nt + n5) * 0x18;
    if (size != expected) throw FormatError("zone: the arrays end at " + std::to_string(expected) + ", the file says " + std::to_string(size));
    if (e.size() < size) throw FormatError("zone: events file shorter than its arrays");
    size_t at = 12;
    ReadArray(e, at, nf, 0x14, z.Furniture);
    ReadArray(e, at, nn, 0x30, z.Characters);
    ReadArray(e, at, nw, 0x18, z.Doors);
    ReadArray(e, at, nt, 0x18, z.Triggers);
    ReadArray(e, at, n5, 0x18, z.Others);
    if (e.size() > size)
    {
        // a Pawn script starting with its own length
        if (e.size() < size + 4) throw FormatError("zone: events file's tail too short");
        const uint32_t length = U32(e, size);
        if (size + length > e.size()) throw FormatError("zone: the initialisation script runs past its file");
        z.InitScript = Slice(e, size, length);
    }
    z.Script = c.Files[2];
    if (c.Files.size() > 4) z.Trailer = c.Files[4];
    return z;
}

}
