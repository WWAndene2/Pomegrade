#include "PicaTexture.h"

#include <algorithm>

namespace remake
{

static const int Bpp[14] = {32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8};
static const int Swizzle[64] = {
    0,  1,  8,  9,  2,  3,  10, 11, 16, 17, 24, 25, 18, 19, 26, 27,
    4,  5,  12, 13, 6,  7,  14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63};

size_t PicaTextureLength(uint32_t width, uint32_t height, uint8_t format)
{
    if (format >= 14) throw FormatError("PICA texture: unknown format " + std::to_string(format));
    const size_t len = (size_t)width * height * Bpp[format] / 8;
    return (len + 0x7F) & ~(size_t)0x7F;
}

static void Etc1Block(uint64_t block, uint8_t rgb[16][3])
{
    static const int Lut[8][4] = {{2, 8, -2, -8}, {5, 17, -5, -17}, {9, 29, -9, -29}, {13, 42, -13, -42},
                                  {18, 60, -18, -60}, {24, 80, -24, -80}, {33, 106, -33, -106}, {47, 183, -47, -183}};
    const uint32_t lo = (uint32_t)(block >> 32), hi = (uint32_t)block;
    const bool flip = hi & 0x1000000, diff = hi & 0x2000000;
    int r1, g1, b1, r2, g2, b2;
    if (diff)
    {
        b1 = hi & 0xF8; g1 = (hi & 0xF800) >> 8; r1 = (hi & 0xF80000) >> 16;
        auto delta = [](uint32_t v) { int d = (int)(v & 7); return d >= 4 ? d - 8 : d; };
        b2 = (b1 >> 3) + delta(hi); g2 = (g1 >> 3) + delta(hi >> 8); r2 = (r1 >> 3) + delta(hi >> 16);
        b1 |= b1 >> 5; g1 |= g1 >> 5; r1 |= r1 >> 5;
        b2 = (b2 << 3) | (b2 >> 2); g2 = (g2 << 3) | (g2 >> 2); r2 = (r2 << 3) | (r2 >> 2);
    }
    else
    {
        b1 = hi & 0xF0; g1 = (hi & 0xF000) >> 8; r1 = (hi & 0xF00000) >> 16;
        b2 = (hi & 0x0F) << 4; g2 = (hi & 0x0F00) >> 4; r2 = (hi & 0x0F0000) >> 12;
        b1 |= b1 >> 4; g1 |= g1 >> 4; r1 |= r1 >> 4; b2 |= b2 >> 4; g2 |= g2 >> 4; r2 |= r2 >> 4;
    }
    const uint32_t t1 = (hi >> 29) & 7, t2 = (hi >> 26) & 7;
    auto pixel = [&](int r, int g, int b, int x, int y, uint32_t table, uint8_t* out) {
        const int index = x * 4 + y;
        const uint32_t msb = lo << 1;
        const int d = index < 8 ? Lut[table][((lo >> (index + 24)) & 1) + ((msb >> (index + 8)) & 2)]
                                : Lut[table][((lo >> (index + 8)) & 1) + ((msb >> (index - 8)) & 2)];
        // ETC1 (Khronos) holds R, G, B, then the flags, big-endian; the 3DS stores the block
        // little-endian, so after the byte swap the base colour's bytes run B, G, R from the high
        // one: what SPICA's names call "r" (bits 16-23) is blue, "b" (bits 0-7) red. Checked on
        // Omega Ruby: read the other way, its character textures have blue skin and a blue
        // outfit; this way, skin tones and a red outfit
        out[0] = (uint8_t)std::clamp(b + d, 0, 255); out[1] = (uint8_t)std::clamp(g + d, 0, 255); out[2] = (uint8_t)std::clamp(r + d, 0, 255);
    };
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
        {
            const bool second = flip ? y >= 2 : x >= 2;
            pixel(second ? r2 : r1, second ? g2 : g1, second ? b2 : b1, x, y, second ? t2 : t1, rgb[y * 4 + x]);
        }
}

Bytes PicaTextureEncodeRgba8(const Bytes& rgba, uint32_t w, uint32_t h)
{
    if (w % 8 || h % 8 || !w || !h) throw FormatError("PICA texture: size not a multiple of 8");
    if (rgba.size() < (size_t)w * h * 4) throw FormatError("PICA texture: fewer pixels than its size");
    Bytes out(PicaTextureLength(w, h, 0), 0);
    size_t at = 0;
    for (uint32_t ty = 0; ty < h; ty += 8)
        for (uint32_t tx = 0; tx < w; tx += 8)
            for (int px = 0; px < 64; px++, at += 4)
            {
                // texel (x, y) counted from the bottom row, as PicaTextureDecode reads it
                const uint32_t x = tx + (Swizzle[px] & 7), yUp = ty + (Swizzle[px] >> 3);
                const uint8_t* p = &rgba[((size_t)(h - 1 - yUp) * w + x) * 4];
                out[at] = p[3]; out[at + 1] = p[2]; out[at + 2] = p[1]; out[at + 3] = p[0];
            }
    return out;
}

Bytes PicaTextureDecode(const Bytes& in, uint32_t w, uint32_t h, uint8_t format)
{
    if (format >= 14) throw FormatError("PICA texture: unknown format " + std::to_string(format));
    if (w % 8 || h % 8 || !w || !h) throw FormatError("PICA texture: size not a multiple of 8");
    if (in.size() < (size_t)w * h * Bpp[format] / 8) throw FormatError("PICA texture: data shorter than its size");
    Bytes out((size_t)w * h * 4);
    // texel (x, y) counted from the bottom row, as the GPU stores it; written top-down
    auto put = [&](uint32_t x, uint32_t yUp, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        uint8_t* p = &out[((size_t)(h - 1 - yUp) * w + x) * 4];
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    };
    if (format == 12 || format == 13) // ETC1, ETC1A4
    {
        static const int Xt[4] = {0, 4, 0, 4}, Yt[4] = {0, 0, 4, 4};
        size_t at = 0;
        for (uint32_t ty = 0; ty < h; ty += 8)
            for (uint32_t tx = 0; tx < w; tx += 8)
                for (int t = 0; t < 4; t++)
                {
                    uint64_t alpha = ~0ull;
                    if (format == 13) { alpha = (uint64_t)U32(in, at) | (uint64_t)U32(in, at + 4) << 32; at += 8; }
                    const uint64_t colour = (uint64_t)U32(in, at) | (uint64_t)U32(in, at + 4) << 32; // swapped below
                    at += 8;
                    uint64_t swapped = 0;
                    for (int k = 0; k < 8; k++) swapped |= ((colour >> (8 * k)) & 0xFF) << (8 * (7 - k));
                    uint8_t rgb[16][3];
                    Etc1Block(swapped, rgb);
                    for (int py = 0; py < 4; py++)
                        for (int px = 0; px < 4; px++)
                        {
                            const int ax = Xt[t] + px, ay = Yt[t] + py;
                            const uint8_t a4 = (alpha >> ((((ax & 3) * 4 + (ay & 3)) << 2))) & 0xF;
                            // ETC1 rows run top-down inside the tile; the tile's rows are counted from the
                            // bottom, as the uncompressed formats' (SPICA writes row Height-1-(TY+PY))
                            put(tx + ax, ty + ay, rgb[py * 4 + px][0], rgb[py * 4 + px][1], rgb[py * 4 + px][2], (uint8_t)(a4 << 4 | a4));
                        }
                }
        return out;
    }
    const int step = std::max(1, Bpp[format] / 8);
    size_t at = 0;
    for (uint32_t ty = 0; ty < h; ty += 8)
        for (uint32_t tx = 0; tx < w; tx += 8)
            for (int px = 0; px < 64; px++)
            {
                const uint32_t x = tx + (Swizzle[px] & 7), y = ty + (Swizzle[px] >> 3);
                auto u16 = [&] { return (uint16_t)(in[at] | in[at + 1] << 8); };
                uint8_t r = 0, g = 0, b = 0, a = 255;
                switch (format)
                {
                case 0: a = in[at]; b = in[at + 1]; g = in[at + 2]; r = in[at + 3]; break;      // RGBA8, stored ABGR
                case 1: b = in[at]; g = in[at + 1]; r = in[at + 2]; break;                       // RGB8
                case 2: { const uint16_t v = u16(); r = ((v >> 11) & 31) * 255 / 31; g = ((v >> 6) & 31) * 255 / 31; b = ((v >> 1) & 31) * 255 / 31; a = (v & 1) * 255; break; }
                case 3: { const uint16_t v = u16(); r = ((v >> 11) & 31) * 255 / 31; g = ((v >> 5) & 63) * 255 / 63; b = (v & 31) * 255 / 31; break; }
                case 4: { const uint16_t v = u16(); r = ((v >> 12) & 15) * 17; g = ((v >> 8) & 15) * 17; b = ((v >> 4) & 15) * 17; a = (v & 15) * 17; break; }
                case 5: r = g = b = in[at + 1]; a = in[at]; break;                               // LA8
                case 6: r = in[at + 1]; g = in[at]; break;                                       // HiLo8
                case 7: r = g = b = in[at]; break;                                               // L8
                case 8: r = g = b = 255; a = in[at]; break;                                      // A8
                case 9: r = g = b = (in[at] >> 4) * 17; a = (in[at] & 15) * 17; break;           // LA4
                case 10: case 11:                                                                // L4, A4: two texels a byte
                {
                    const int nib = (in[at >> 1] >> ((at & 1) * 4)) & 15;
                    if (format == 10) r = g = b = nib * 17; else { r = g = b = 255; a = nib * 17; }
                    break;
                }
                }
                put(x, y, r, g, b, a);
                at += (format == 10 || format == 11) ? 1 : step;
            }
    return out;
}

}
