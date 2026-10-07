#include "PicaTexture.h"

#include <algorithm>
#include <climits>
#include <cmath>

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

namespace remake
{

// ETC1: the 64-bit block as Etc1Block reads it (after the byte swap). Its colour fields named as Etc1Block names them:
// "b" (bits 0-7) is the decoded red, "g" green, "r" (bits 16-23) the decoded blue (see the note in Etc1Block)
static const int Etc1Lut[8][4] = {{2, 8, -2, -8}, {5, 17, -5, -17}, {9, 29, -9, -29}, {13, 42, -13, -42},
                                  {18, 60, -18, -60}, {24, 80, -24, -80}, {33, 106, -33, -106}, {47, 183, -47, -183}};

// a half block's best table and per-pixel modifiers for a base colour (decoded R, G, B), with its squared error
struct Etc1Half
{
    int table = 0;
    int mods[16] = {};
    long error = 0;
};

static Etc1Half FitHalf(const uint8_t px[16][4], const bool in[16], const int base[3])
{
    Etc1Half best;
    best.error = LONG_MAX;
    for (int t = 0; t < 8; t++)
    {
        Etc1Half h;
        h.table = t;
        for (int i = 0; i < 16 && h.error < best.error; i++)
        {
            if (!in[i]) continue;
            long bestPixel = LONG_MAX;
            for (int m = 0; m < 4; m++)
            {
                long e = 0;
                for (int c = 0; c < 3; c++)
                {
                    const int v = std::clamp(base[c] + Etc1Lut[t][m], 0, 255) - px[i][c];
                    e += (long)v * v;
                }
                if (e < bestPixel) { bestPixel = e; h.mods[i] = m; }
            }
            h.error += bestPixel;
        }
        if (h.error < best.error) best = h;
    }
    return best;
}

// the block (Etc1Block's bit layout) closest to 16 pixels, px[y * 4 + x], RGB
static uint64_t Etc1Encode(const uint8_t px[16][4])
{
    long bestError = LONG_MAX;
    uint64_t bestBlock = 0;
    for (int flip = 0; flip < 2; flip++)
    {
        bool in[2][16];
        long sum[2][3] = {};
        int count[2] = {};
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
            {
                const int i = y * 4 + x, half = flip ? (y >= 2) : (x >= 2);
                in[half][i] = true; in[1 - half][i] = false;
                for (int c = 0; c < 3; c++) sum[half][c] += px[i][c];
                count[half]++;
            }
        for (int diff = 0; diff < 2; diff++)
        {
            // each half's base colour quantized (4 bits, or 5 with the second half a 3-bit delta from the first),
            // tried at the nearest level to the half's mean and up to Reach levels either side per channel: the mean
            // includes the modifiers, so the best base can lie a few levels off it
            const int bits = diff ? 5 : 4, levels = (1 << bits) - 1;
            int q[2][3];
            for (int h = 0; h < 2; h++)
                for (int c = 0; c < 3; c++)
                    q[h][c] = (int)std::lround((double)sum[h][c] / count[h] * levels / 255.0);
            auto expand = [&](int v) { return bits == 4 ? v * 17 : (v << 3) | (v >> 2); };
            Etc1Half fit[2];
            int chosen[2][3];
            long total = 0;
            for (int h = 0; h < 2; h++)
            {
                fit[h].error = LONG_MAX;
                const int Reach = 3;
                for (int dr = -Reach; dr <= Reach; dr++)
                    for (int dg = -Reach; dg <= Reach; dg++)
                        for (int db = -Reach; db <= Reach; db++)
                        {
                            const int c3[3] = {q[h][0] + dr, q[h][1] + dg, q[h][2] + db};
                            bool ok = true;
                            for (int c = 0; c < 3; c++)
                            {
                                if (c3[c] < 0 || c3[c] > levels) ok = false;
                                // differential: the second half within -4..3 of the first's chosen colour
                                if (ok && diff && h == 1 && (c3[c] - chosen[0][c] < -4 || c3[c] - chosen[0][c] > 3)) ok = false;
                            }
                            if (!ok) continue;
                            const int base[3] = {expand(c3[0]), expand(c3[1]), expand(c3[2])};
                            const Etc1Half f = FitHalf(px, in[h], base);
                            if (f.error < fit[h].error) { fit[h] = f; for (int c = 0; c < 3; c++) chosen[h][c] = c3[c]; }
                        }
                if (fit[h].error == LONG_MAX) { total = LONG_MAX; break; }
                total += fit[h].error;
            }
            if (total >= bestError) continue;
            // the block: colours (decoded R in Etc1Block's "b" field, B in its "r" field), flags, tables, then the
            // pixel indices (index x * 4 + y; its low bit and high bit placed as Etc1Block reads them)
            uint32_t hi = 0, lo = 0;
            const int field[3] = {0, 8, 16}; // decoded R, G, B
            for (int c = 0; c < 3; c++)
            {
                if (diff)
                {
                    hi |= (uint32_t)chosen[0][c] << (field[c] + 3);
                    hi |= (uint32_t)((chosen[1][c] - chosen[0][c]) & 7) << field[c];
                }
                else
                {
                    hi |= (uint32_t)chosen[0][c] << (field[c] + 4);
                    hi |= (uint32_t)chosen[1][c] << field[c];
                }
            }
            hi |= (uint32_t)flip << 24 | (uint32_t)diff << 25 | (uint32_t)fit[1].table << 26 | (uint32_t)fit[0].table << 29;
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++)
                {
                    const int i = y * 4 + x, half = flip ? (y >= 2) : (x >= 2), index = x * 4 + y;
                    const int m = fit[half].mods[i]; // Lut column: low bit + 2 * high bit
                    const int lowBit = index < 8 ? index + 24 : index + 8, highBit = index < 8 ? index + 8 : index - 8;
                    lo |= (uint32_t)(m & 1) << lowBit | (uint32_t)(m >> 1) << highBit;
                }
            bestError = total;
            bestBlock = (uint64_t)lo << 32 | hi;
        }
    }
    return bestBlock;
}

Bytes PicaTextureEncode(const Bytes& rgba, uint32_t w, uint32_t h, uint8_t format)
{
    if (format >= 14) throw FormatError("PICA texture: unknown format " + std::to_string(format));
    if (w % 8 || h % 8 || !w || !h) throw FormatError("PICA texture: size not a multiple of 8");
    if (rgba.size() < (size_t)w * h * 4) throw FormatError("PICA texture: fewer pixels than its size");
    if (format == 0) return PicaTextureEncodeRgba8(rgba, w, h);
    Bytes out(PicaTextureLength(w, h, format), 0);
    // texel (x, y) counted from the bottom row, as the decoder reads it
    auto texel = [&](uint32_t x, uint32_t yUp) { return &rgba[((size_t)(h - 1 - yUp) * w + x) * 4]; };
    auto level = [](int v, int levels) { return (int)std::lround(v * levels / 255.0); };
    auto luma = [](const uint8_t* p) { return (p[0] * 299 + p[1] * 587 + p[2] * 114 + 500) / 1000; };
    size_t at = 0;
    if (format == 12 || format == 13)
    {
        static const int Xt[4] = {0, 4, 0, 4}, Yt[4] = {0, 0, 4, 4};
        for (uint32_t ty = 0; ty < h; ty += 8)
            for (uint32_t tx = 0; tx < w; tx += 8)
                for (int t = 0; t < 4; t++)
                {
                    uint8_t px[16][4];
                    uint64_t alpha = 0;
                    for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                        {
                            const uint8_t* p = texel(tx + Xt[t] + x, ty + Yt[t] + y);
                            for (int c = 0; c < 4; c++) px[y * 4 + x][c] = p[c];
                            alpha |= (uint64_t)level(p[3], 15) << ((x * 4 + y) << 2);
                        }
                    if (format == 13)
                    {
                        for (int k = 0; k < 8; k++) out[at + k] = (uint8_t)(alpha >> (8 * k));
                        at += 8;
                    }
                    // stored little-endian, the bytes in the reverse order of the block Etc1Block reads
                    const uint64_t block = Etc1Encode(px);
                    for (int k = 0; k < 8; k++) out[at + k] = (uint8_t)(block >> (8 * (7 - k)));
                    at += 8;
                }
        return out;
    }
    for (uint32_t ty = 0; ty < h; ty += 8)
        for (uint32_t tx = 0; tx < w; tx += 8)
            for (int px = 0; px < 64; px++)
            {
                const uint8_t* p = texel(tx + (Swizzle[px] & 7), ty + (Swizzle[px] >> 3));
                auto put16 = [&](uint32_t v) { out[at] = (uint8_t)v; out[at + 1] = (uint8_t)(v >> 8); };
                switch (format)
                {
                case 1: out[at] = p[2]; out[at + 1] = p[1]; out[at + 2] = p[0]; at += 3; break;
                case 2: put16((uint32_t)level(p[0], 31) << 11 | (uint32_t)level(p[1], 31) << 6 | (uint32_t)level(p[2], 31) << 1 | (p[3] >= 128)); at += 2; break;
                case 3: put16((uint32_t)level(p[0], 31) << 11 | (uint32_t)level(p[1], 63) << 5 | (uint32_t)level(p[2], 31)); at += 2; break;
                case 4: put16((uint32_t)level(p[0], 15) << 12 | (uint32_t)level(p[1], 15) << 8 | (uint32_t)level(p[2], 15) << 4 | (uint32_t)level(p[3], 15)); at += 2; break;
                case 5: out[at] = p[3]; out[at + 1] = (uint8_t)luma(p); at += 2; break;
                case 6: out[at + 1] = p[0]; out[at] = p[1]; at += 2; break;
                case 7: out[at] = (uint8_t)luma(p); at += 1; break;
                case 8: out[at] = p[3]; at += 1; break;
                case 9: out[at] = (uint8_t)(level(luma(p), 15) << 4 | level(p[3], 15)); at += 1; break;
                case 10: case 11:
                {
                    const int nib = format == 10 ? level(luma(p), 15) : level(p[3], 15);
                    out[at >> 1] |= (uint8_t)(nib << ((at & 1) * 4));
                    at += 1;
                    break;
                }
                }
            }
    return out;
}

}
