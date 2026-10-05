#include "NitroCompression.h"

#include <algorithm>
#include <vector>

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

Bytes Lz11Compress(const Bytes& data)
{
    constexpr size_t Window = 0x1000, MaxLen = 0x10110, Chain = 64;
    Bytes out;
    if (data.size() <= 0xFFFFFF) { out = {0x11, (uint8_t)(data.size()), (uint8_t)(data.size() >> 8), (uint8_t)(data.size() >> 16)}; }
    else { out = {0x11, 0, 0, 0}; for (int i = 0; i < 4; i++) out.push_back((uint8_t)(data.size() >> (8 * i))); }

    // the latest position of each 3-byte sequence, and the previous one with the same hash
    std::vector<int64_t> head(1 << 16, -1), prev(data.size(), -1);
    auto hash = [&](size_t i) { return (size_t)((data[i] << 8 ^ data[i + 1] << 4 ^ data[i + 2]) & 0xFFFF); };
    auto insert = [&](size_t i) { if (i + 2 < data.size()) { const size_t h = hash(i); prev[i] = head[h]; head[h] = (int64_t)i; } };

    size_t at = 0;
    while (at < data.size())
    {
        const size_t flagsAt = out.size();
        out.push_back(0);
        for (int bit = 7; bit >= 0 && at < data.size(); bit--)
        {
            size_t bestLen = 0, bestDisp = 0;
            if (at + 2 < data.size())
            {
                size_t steps = 0;
                for (int64_t c = head[hash(at)]; c >= 0 && at - (size_t)c <= Window && steps < Chain; c = prev[(size_t)c], steps++)
                {
                    size_t len = 0;
                    const size_t limit = std::min(MaxLen, data.size() - at);
                    while (len < limit && data[(size_t)c + len] == data[at + len]) len++;
                    if (len > bestLen) { bestLen = len; bestDisp = at - (size_t)c; if (len == limit) break; }
                }
            }
            if (bestLen < 3)
            {
                out.push_back(data[at]);
                insert(at++);
                continue;
            }
            out[flagsAt] |= (uint8_t)(1 << bit);
            const size_t d = bestDisp - 1;
            if (bestLen <= 0x10)
            {
                out.push_back((uint8_t)((bestLen - 1) << 4 | d >> 8)); out.push_back((uint8_t)d);
            }
            else if (bestLen <= 0x110)
            {
                const size_t l = bestLen - 0x11;
                out.push_back((uint8_t)(l >> 4)); out.push_back((uint8_t)((l & 0xF) << 4 | d >> 8)); out.push_back((uint8_t)d);
            }
            else
            {
                const size_t l = bestLen - 0x111;
                out.push_back((uint8_t)(0x10 | l >> 12)); out.push_back((uint8_t)(l >> 4));
                out.push_back((uint8_t)((l & 0xF) << 4 | d >> 8)); out.push_back((uint8_t)d);
            }
            for (size_t k = 0; k < bestLen; k++) insert(at++);
        }
    }
    return out;
}

}
