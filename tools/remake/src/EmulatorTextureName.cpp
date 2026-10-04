#include "EmulatorTextureName.h"

#include "xxhash/xxhash.h"

#include <cinttypes>
#include <cstdio>

namespace remake
{

static uint32_t Rgb6(uint16_t v)
{
    uint8_t r = (v & 0x1F) << 1, g = (v & 0x3E0) >> 4, b = (v & 0x7C00) >> 9;
    if (r) r++;
    if (g) g++;
    if (b) b++;
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

// a palette colour; the 4x4 format may address past what the file holds
static uint16_t Pal(const Bytes& p, size_t i) { return i * 2 + 1 < p.size() ? U16(p, i * 2) : 0; }

static uint16_t Mix(uint16_t c0, uint16_t c1, uint32_t w0, uint32_t w1, uint32_t shift)
{
    const uint32_t r = ((c0 & 0x1F) * w0 + (c1 & 0x1F) * w1) >> shift;
    const uint32_t g = (((c0 & 0x3E0) * w0 + (c1 & 0x3E0) * w1) >> shift) & 0x3E0;
    const uint32_t b = (((c0 & 0x7C00) * w0 + (c1 & 0x7C00) * w1) >> shift) & 0x7C00;
    return (uint16_t)(r | g | b);
}

std::vector<uint32_t> DecodeRgb6a5(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo)
{
    const uint32_t w = f.Width, h = f.Height;
    std::vector<uint32_t> out((size_t)w * h);
    switch (f.Format)
    {
    case 1: case 6: // A3I5, A5I3
    {
        const int indexBits = f.Format == 1 ? 5 : 3;
        for (size_t i = 0; i < out.size(); i++)
        {
            const uint8_t v = U8(texels, i);
            uint32_t a = v >> indexBits;
            if (f.Format == 1) a = a * 4 + a / 2;
            out[i] = Rgb6(Pal(palette, v & ((1 << indexBits) - 1))) | a << 24;
        }
        break;
    }
    case 2: case 3: case 4:
    {
        const int bits = f.Format == 2 ? 2 : f.Format == 3 ? 4 : 8;
        for (size_t i = 0; i < out.size(); i++)
        {
            const uint32_t index = (U8(texels, i * bits / 8) >> (i * bits % 8)) & ((1 << bits) - 1);
            out[i] = Rgb6(Pal(palette, index)) | (f.Colour0Transparent && index == 0 ? 0 : 0x1F000000);
        }
        break;
    }
    case 5:
        for (uint32_t by = 0; by < h / 4; by++)
            for (uint32_t bx = 0; bx < w / 4; bx++)
            {
                const size_t block = bx + by * (w / 4);
                const uint32_t data = U32(texels, block * 4);
                const uint16_t aux = U16(blockInfo, block * 2);
                const size_t base = (size_t)(aux & 0x3FFF) * 2;
                uint16_t c[4] = {(uint16_t)(Pal(palette, base) | 0x8000), (uint16_t)(Pal(palette, base + 1) | 0x8000),
                                 (uint16_t)(Pal(palette, base + 2) | 0x8000), (uint16_t)(Pal(palette, base + 3) | 0x8000)};
                switch (aux >> 14)
                {
                case 0: c[3] = 0; break;
                case 1: c[2] = Mix(c[0], c[1], 1, 1, 1) | 0x8000; c[3] = 0; break;
                case 2: break;
                case 3: c[2] = Mix(c[0], c[1], 5, 3, 3) | 0x8000; c[3] = Mix(c[0], c[1], 3, 5, 3) | 0x8000; break;
                }
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++)
                    {
                        const uint16_t colour = c[(data >> 2 * (i + j * 4)) & 3];
                        out[bx * 4 + i + (by * 4 + j) * w] = Rgb6(colour) | (colour & 0x8000 ? 0x1F000000 : 0);
                    }
            }
        break;
    case 7:
        for (size_t i = 0; i < out.size(); i++)
        {
            const uint16_t v = U16(texels, i * 2);
            out[i] = Rgb6(v) | (v & 0x8000 ? 0x1F000000 : 0);
        }
        break;
    default:
        throw FormatError("texture format " + std::to_string(f.Format));
    }
    return out;
}

std::string EmulatorTextureName(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo)
{
    const std::vector<uint32_t> d = DecodeRgb6a5(f, texels, palette, blockInfo);
    // the core hashes its u32 buffer in memory: little-endian on every device it runs on
    const uint64_t hash = XXH64(d.data(), d.size() * 4, ((uint64_t)f.Width << 32) | f.Height);
    char name[64];
    snprintf(name, sizeof name, "tex_%ux%u_%016" PRIx64, f.Width, f.Height, hash);
    return name;
}

}
