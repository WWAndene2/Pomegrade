#include "Png.h"

namespace remake
{

static uint32_t Crc32(const uint8_t* p, size_t n, uint32_t crc = 0)
{
    crc = ~crc;
    for (size_t i = 0; i < n; i++)
    {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

static void Be32(Bytes& o, uint32_t v) { for (int i = 3; i >= 0; i--) o.push_back((v >> (8 * i)) & 0xFF); }

static void Chunk(Bytes& o, const char* type, const Bytes& body)
{
    Be32(o, (uint32_t)body.size());
    const size_t start = o.size();
    o.insert(o.end(), type, type + 4);
    o.insert(o.end(), body.begin(), body.end());
    Be32(o, Crc32(&o[start], o.size() - start));
}

Bytes EncodePng(uint32_t width, uint32_t height, const Bytes& rgba)
{
    if (rgba.size() != (size_t)width * height * 4) throw FormatError("PNG: pixel count does not match the size");
    // raw scanlines, each with filter byte 0
    Bytes raw;
    raw.reserve((size_t)(width * 4 + 1) * height);
    for (uint32_t y = 0; y < height; y++)
    {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + (size_t)y * width * 4, rgba.begin() + (size_t)(y + 1) * width * 4);
    }
    // zlib: header, stored blocks of up to 65535 bytes, Adler-32
    Bytes z = {0x78, 0x01};
    for (size_t at = 0; at < raw.size() || at == 0; )
    {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        const bool last = at + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
        at += n;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    Be32(z, (b << 16) | a);

    Bytes png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    Bytes ihdr;
    Be32(ihdr, width); Be32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0}); // 8 bits, RGBA, deflate, no filter, no interlace
    Chunk(png, "IHDR", ihdr);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", {});
    return png;
}

}
