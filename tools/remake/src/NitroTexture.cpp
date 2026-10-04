#include "NitroTexture.h"

namespace remake
{

void Bgr555ToRgb(uint16_t c, uint8_t* rgb)
{
    for (int k = 0; k < 3; k++)
    {
        const uint8_t v = (c >> (5 * k)) & 31;
        rgb[k] = (uint8_t)(v << 3 | v >> 2);
    }
}

static uint16_t Colour(const Bytes& palette, size_t i)
{
    return U16(palette, i * 2);
}

Bytes DecodeTexture(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo)
{
    const uint32_t w = f.Width, h = f.Height;
    if (w == 0 || h == 0 || w > 1024 || h > 1024) throw FormatError("texture: bad size");
    Bytes out((size_t)w * h * 4, 0);
    auto put = [&](uint32_t x, uint32_t y, uint16_t c, uint8_t alpha) {
        uint8_t* p = &out[((size_t)y * w + x) * 4];
        Bgr555ToRgb(c, p);
        p[3] = alpha;
    };
    switch (f.Format)
    {
    case 1: case 6: // A3I5, A5I3
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++)
            {
                const uint8_t b = U8(texels, (size_t)y * w + x);
                const uint8_t index = f.Format == 1 ? (b & 31) : (b & 7);
                const uint8_t a = f.Format == 1 ? (uint8_t)((b >> 5) * 36 + (b >> 5) / 2) : (uint8_t)(((b >> 3) << 3) | (b >> 5));
                put(x, y, Colour(palette, index), a);
            }
        break;
    case 2: case 3: case 4: // 2, 4, 8 bits a texel
    {
        const int bits = f.Format == 2 ? 2 : f.Format == 3 ? 4 : 8;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++)
            {
                const size_t bit = ((size_t)y * w + x) * bits;
                const uint8_t index = (U8(texels, bit / 8) >> (bit % 8)) & ((1 << bits) - 1);
                put(x, y, Colour(palette, index), index == 0 && f.Colour0Transparent ? 0 : 255);
            }
        break;
    }
    case 5: // 4x4 blocks: a u32 of 2-bit texels, a u16 of palette offset and mode
        for (uint32_t by = 0; by < h / 4; by++)
            for (uint32_t bx = 0; bx < w / 4; bx++)
            {
                const size_t block = (size_t)by * (w / 4) + bx;
                const uint32_t bits = U32(texels, block * 4);
                const uint16_t info = U16(blockInfo, block * 2);
                const size_t base = (size_t)(info & 0x3FFF) * 2; // in colours: offset << 2 bytes
                const int mode = info >> 14;
                const uint16_t c0 = Colour(palette, base), c1 = Colour(palette, base + 1);
                auto mix = [](uint16_t a, uint16_t b, int wa, int wb, int d) {
                    uint16_t r = 0;
                    for (int k = 0; k < 3; k++)
                        r |= (uint16_t)(((((a >> (5 * k)) & 31) * wa + ((b >> (5 * k)) & 31) * wb) / d) << (5 * k));
                    return r;
                };
                for (int t = 0; t < 16; t++)
                {
                    const int c = (bits >> (t * 2)) & 3;
                    uint16_t colour = 0; uint8_t a = 255;
                    if (c == 0) colour = c0;
                    else if (c == 1) colour = c1;
                    else if (c == 2) colour = mode == 1 ? mix(c0, c1, 1, 1, 2) : mode == 3 ? mix(c0, c1, 5, 3, 8) : Colour(palette, base + 2);
                    else if (mode == 2) colour = Colour(palette, base + 3);
                    else if (mode == 3) colour = mix(c0, c1, 3, 5, 8);
                    else a = 0;
                    put(bx * 4 + t % 4, by * 4 + t / 4, colour, a);
                }
            }
        break;
    case 7:
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++)
            {
                const uint16_t c = U16(texels, ((size_t)y * w + x) * 2);
                put(x, y, c, c & 0x8000 ? 255 : 0);
            }
        break;
    default:
        throw FormatError("texture: unknown format " + std::to_string(f.Format));
    }
    return out;
}

}
