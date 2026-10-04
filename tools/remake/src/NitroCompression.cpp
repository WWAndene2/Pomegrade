#include "NitroCompression.h"

namespace remake
{

bool IsLzCompressed(const Bytes& data)
{
    if (data.size() < 5 || (data[0] != 0x10 && data[0] != 0x11)) return false;
    uint32_t size = U32(data, 0) >> 8;
    if (size == 0 && data[0] == 0x11 && data.size() >= 8) size = U32(data, 4);
    // compression saves at most 8 bytes per 9 (LZ11: a little more); a
    // "decompressed size" far beyond that, or zero, is not one
    return size > 0 && size <= data.size() * 64u;
}

Bytes LzDecompress(const Bytes& data)
{
    const uint8_t type = U8(data, 0);
    if (type != 0x10 && type != 0x11) throw FormatError("not LZ10/LZ11 data");
    size_t size = U32(data, 0) >> 8, at = 4;
    if (size == 0 && type == 0x11) { size = U32(data, 4); at = 8; }
    Bytes out;
    out.reserve(size);
    while (out.size() < size)
    {
        const uint8_t flags = U8(data, at++);
        for (int bit = 7; bit >= 0 && out.size() < size; bit--)
        {
            if (!(flags >> bit & 1)) { out.push_back(U8(data, at++)); continue; }
            size_t len, disp;
            const uint8_t b0 = U8(data, at);
            if (type == 0x10)
            {
                const uint8_t b1 = U8(data, at + 1);
                len = (b0 >> 4) + 3;
                disp = (((b0 & 0xF) << 8) | b1) + 1;
                at += 2;
            }
            else if ((b0 >> 4) == 0)
            {
                const uint8_t b1 = U8(data, at + 1), b2 = U8(data, at + 2);
                len = (((b0 & 0xF) << 4) | (b1 >> 4)) + 0x11;
                disp = (((b1 & 0xF) << 8) | b2) + 1;
                at += 3;
            }
            else if ((b0 >> 4) == 1)
            {
                const uint8_t b1 = U8(data, at + 1), b2 = U8(data, at + 2), b3 = U8(data, at + 3);
                len = (((b0 & 0xF) << 12) | (b1 << 4) | (b2 >> 4)) + 0x111;
                disp = (((b2 & 0xF) << 8) | b3) + 1;
                at += 4;
            }
            else
            {
                const uint8_t b1 = U8(data, at + 1);
                len = (b0 >> 4) + 1;
                disp = (((b0 & 0xF) << 8) | b1) + 1;
                at += 2;
            }
            if (disp > out.size()) throw FormatError("LZ: reference before the start of the data");
            for (size_t i = 0; i < len && out.size() < size; i++) out.push_back(out[out.size() - disp]);
        }
    }
    return out;
}

}
