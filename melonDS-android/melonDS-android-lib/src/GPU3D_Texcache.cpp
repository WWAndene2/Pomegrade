#include "GPU3D_Texcache.h"

namespace melonDS
{

inline u16 ColorAvg(u16 color0, u16 color1)
{
    u32 r0 = color0 & 0x001F;
    u32 g0 = color0 & 0x03E0;
    u32 b0 = color0 & 0x7C00;
    u32 r1 = color1 & 0x001F;
    u32 g1 = color1 & 0x03E0;
    u32 b1 = color1 & 0x7C00;

    u32 r = (r0 + r1) >> 1;
    u32 g = ((g0 + g1) >> 1) & 0x03E0;
    u32 b = ((b0 + b1) >> 1) & 0x7C00;

    return r | g | b;
}

inline u16 Color5of3(u16 color0, u16 color1)
{
    u32 r0 = color0 & 0x001F;
    u32 g0 = color0 & 0x03E0;
    u32 b0 = color0 & 0x7C00;
    u32 r1 = color1 & 0x001F;
    u32 g1 = color1 & 0x03E0;
    u32 b1 = color1 & 0x7C00;

    u32 r = (r0*5 + r1*3) >> 3;
    u32 g = ((g0*5 + g1*3) >> 3) & 0x03E0;
    u32 b = ((b0*5 + b1*3) >> 3) & 0x7C00;

    return r | g | b;
}

inline u16 Color3of5(u16 color0, u16 color1)
{
    u32 r0 = color0 & 0x001F;
    u32 g0 = color0 & 0x03E0;
    u32 b0 = color0 & 0x7C00;
    u32 r1 = color1 & 0x001F;
    u32 g1 = color1 & 0x03E0;
    u32 b1 = color1 & 0x7C00;

    u32 r = (r0*3 + r1*5) >> 3;
    u32 g = ((g0*3 + g1*5) >> 3) & 0x03E0;
    u32 b = ((b0*3 + b1*5) >> 3) & 0x7C00;

    return r | g | b;
}

inline u32 ConvertRGB5ToRGB8(u16 val)
{
    return (((u32)val & 0x1F) << 3)
        | (((u32)val & 0x3E0) << 6)
        | (((u32)val & 0x7C00) << 9);
}
inline u32 ConvertRGB5ToBGR8(u16 val)
{
    return (((u32)val & 0x1F) << 9)
        | (((u32)val & 0x3E0) << 6)
        | (((u32)val & 0x7C00) << 3);
}
inline u32 ConvertRGB5ToRGB6(u16 val)
{
    u8 r = (val & 0x1F) << 1;
    u8 g = (val & 0x3E0) >> 4;
    u8 b = (val & 0x7C00) >> 9;
    if (r) r++;
    if (g) g++;
    if (b) b++;
    return (u32)r | ((u32)g << 8) | ((u32)b << 16);
}

template <int outputFmt>
void ConvertBitmapTexture(u32 width, u32 height, u32* output, u32 addr, GPU& gpu)
{
    for (u32 i = 0; i < width*height; i++)
    {
        u16 value = gpu.ReadVRAMFlat_Texture<u16>(addr + i * 2);

        switch (outputFmt)
        {
        case outputFmt_RGB6A5:
            output[i] = ConvertRGB5ToRGB6(value) | (value & 0x8000 ? 0x1F000000 : 0);
            break;
        case outputFmt_RGBA8:
            output[i] = ConvertRGB5ToRGB8(value) | (value & 0x8000 ? 0xFF000000 : 0);
            break;
        case outputFmt_BGRA8:
            output[i] = ConvertRGB5ToBGR8(value) | (value & 0x8000 ? 0xFF000000 : 0);
            break;
        }
    }
}

template void ConvertBitmapTexture<outputFmt_RGB6A5>(u32 width, u32 height, u32* output, u32 addr, GPU& gpu);

template <int outputFmt>
void ConvertCompressedTexture(u32 width, u32 height, u32* output, u32 addr, u32 addrAux, u32 palAddr, GPU& gpu)
{
    // we process a whole block at the time
    for (int y = 0; y < height / 4; y++)
    {
        for (int x = 0; x < width / 4; x++)
        {
            u32 data = gpu.ReadVRAMFlat_Texture<u32>(addr + (x + y * (width / 4))*4);
            u16 auxData = gpu.ReadVRAMFlat_Texture<u16>(addrAux + (x + y * (width / 4))*2);

            u32 paletteOffset = palAddr + (auxData & 0x3FFF) * 4;
            u16 color0 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset) | 0x8000;
            u16 color1 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+2) | 0x8000;
            u16 color2 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+4) | 0x8000;
            u16 color3 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+6) | 0x8000;

            switch ((auxData >> 14) & 0x3)
            {
            case 0:
                color3 = 0;
                break;
            case 1:
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0 + r1) >> 1;
                    u32 g = ((g0 + g1) >> 1) & 0x03E0;
                    u32 b = ((b0 + b1) >> 1) & 0x7C00;
                    color2 = r | g | b | 0x8000;
                }
                color3 = 0;
                break;
            case 2:
                break;
            case 3:
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*5 + r1*3) >> 3;
                    u32 g = ((g0*5 + g1*3) >> 3) & 0x03E0;
                    u32 b = ((b0*5 + b1*3) >> 3) & 0x7C00;

                    color2 = r | g | b | 0x8000;
                }
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*3 + r1*5) >> 3;
                    u32 g = ((g0*3 + g1*5) >> 3) & 0x03E0;
                    u32 b = ((b0*3 + b1*5) >> 3) & 0x7C00;

                    color3 = r | g | b | 0x8000;
                }
                break;
            }

            // in 2020 our default data types are big enough to be used as lookup tables...
            u64 packed = color0 | ((u64)color1 << 16) | ((u64)color2 << 32) | ((u64)color3 << 48);

            for (int j = 0; j < 4; j++)
            {
                for (int i = 0; i < 4; i++)
                {
                    u32 colorIdx = 16 * ((data >> 2 * (i + j * 4)) & 0x3);
                    u16 color = (packed >> colorIdx) & 0xFFFF;
                    u32 res;
                    switch (outputFmt)
                    {
                    case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color)
                        | ((color & 0x8000) ? 0x1F000000 : 0); break;
                    case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color)
                        | ((color & 0x8000) ? 0xFF000000 : 0); break;
                    case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color)
                        | ((color & 0x8000) ? 0xFF000000 : 0); break;
                    }
                    output[x * 4 + i + (y * 4 + j) * width] = res;
                }
            }
        }
    }
}

template void ConvertCompressedTexture<outputFmt_RGB6A5>(u32, u32, u32*, u32, u32, u32, GPU&);

template <int outputFmt, int X, int Y>
void ConvertAXIYTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, GPU& gpu)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            u8 val = gpu.ReadVRAMFlat_Texture<u8>(addr + x + y * width);

            u32 idx = val & ((1 << Y) - 1);

            u16 color = gpu.ReadVRAMFlat_TexPal<u16>(palAddr + idx * 2);
            u32 alpha = (val >> Y) & ((1 << X) - 1);
            if (X != 5)
                alpha = alpha * 4 + alpha / 2;

            u32 res;
            switch (outputFmt)
            {
            case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color) | alpha << 24; break;
            // make sure full alpha == 255
            case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color) | (alpha << 27 | (alpha & 0x1C) << 22); break;
            case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color) | (alpha << 27 | (alpha & 0x1C) << 22); break;
            }
            output[x + y * width] = res;
        }
    }
}

template void ConvertAXIYTexture<outputFmt_RGB6A5, 5, 3>(u32, u32, u32*, u32, u32, GPU&);
template void ConvertAXIYTexture<outputFmt_RGB6A5, 3, 5>(u32, u32, u32*, u32, u32, GPU&);

template <int outputFmt, int colorBits>
void ConvertNColorsTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, bool color0Transparent, GPU& gpu)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width / (16 / colorBits); x++)
        {
            // smallest possible row is 8 pixels with 2bpp => fits in u16
            u16 val = gpu.ReadVRAMFlat_Texture<u16>(addr + 2 * (x + y * (width / (16 / colorBits))));

            for (int i = 0; i < 16 / colorBits; i++)
            {
                u32 index = val & ((1 << colorBits) - 1);
                val >>= colorBits;
                u16 color = gpu.ReadVRAMFlat_TexPal<u16>(palAddr + index * 2);

                bool transparent = color0Transparent && index == 0;
                u32 res;
                switch (outputFmt)
                {
                case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color)
                    | (transparent ? 0 : 0x1F000000); break;
                case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color)
                    | (transparent ? 0 : 0xFF000000); break;
                case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color)
                    | (transparent ? 0 : 0xFF000000); break;
                }
                output[x * (16 / colorBits) + y * width + i] = res;
            }
        }
    }
}

template void ConvertNColorsTexture<outputFmt_RGB6A5, 2>(u32, u32, u32*, u32, u32, bool, GPU&);
template void ConvertNColorsTexture<outputFmt_RGB6A5, 4>(u32, u32, u32*, u32, u32, bool, GPU&);
template void ConvertNColorsTexture<outputFmt_RGB6A5, 8>(u32, u32, u32*, u32, u32, bool, GPU&);

}
namespace melonDS
{

void DecodeTexture(GPU& gpu, u32 texParam, u32 palBase, u32* output, TexSource& source)
{
    u32 fmt = (texParam >> 26) & 0x7;
    u32 width = TextureWidth(texParam);
    u32 height = TextureHeight(texParam);
    u32 addr = (texParam & 0xFFFF) * 8;

    source = {};
    source.TextureRAMStart[0] = addr;

    if (fmt == 7)
    {
        source.TextureRAMSize[0] = width*height*2;

        ConvertBitmapTexture<outputFmt_RGB6A5>(width, height, output, addr, gpu);
    }
    else if (fmt == 5)
    {
        u32 slot1addr = 0x20000 + ((addr & 0x1FFFC) >> 1);
        if (addr >= 0x40000)
            slot1addr += 0x10000;

        source.TextureRAMSize[0] = width*height/16*4;
        source.TextureRAMStart[1] = slot1addr;
        source.TextureRAMSize[1] = width*height/16*2;
        source.TexPalStart = palBase*16;
        source.TexPalSize = 0x10000;

        ConvertCompressedTexture<outputFmt_RGB6A5>(width, height, output, addr, slot1addr, source.TexPalStart, gpu);
    }
    else
    {
        u32 texSize, palAddr = palBase*16, numPalEntries;
        switch (fmt)
        {
        case 1: texSize = width*height; numPalEntries = 32; break;
        case 6: texSize = width*height; numPalEntries = 8; break;
        case 2: texSize = width*height/4; numPalEntries = 4; palAddr >>= 1; break;
        case 3: texSize = width*height/2; numPalEntries = 16; break;
        default: texSize = width*height; numPalEntries = 256; break; // 4
        }

        palAddr &= 0x1FFFF;

        source.TextureRAMSize[0] = texSize;
        source.TexPalStart = palAddr;
        source.TexPalSize = numPalEntries*2;

        bool color0Transparent = texParam & (1 << 29);

        switch (fmt)
        {
        case 1: ConvertAXIYTexture<outputFmt_RGB6A5, 3, 5>(width, height, output, addr, palAddr, gpu); break;
        case 6: ConvertAXIYTexture<outputFmt_RGB6A5, 5, 3>(width, height, output, addr, palAddr, gpu); break;
        case 2: ConvertNColorsTexture<outputFmt_RGB6A5, 2>(width, height, output, addr, palAddr, color0Transparent, gpu); break;
        case 3: ConvertNColorsTexture<outputFmt_RGB6A5, 4>(width, height, output, addr, palAddr, color0Transparent, gpu); break;
        case 4: ConvertNColorsTexture<outputFmt_RGB6A5, 8>(width, height, output, addr, palAddr, color0Transparent, gpu); break;
        }
    }
}

u64 TexcacheMaskedHash(u8* vram, u32 vramSize, u32 addr, u32 size)
{
    u64 hash = 0;

    while (size > 0)
    {
        u32 pieceSize;
        if (addr + size > vramSize)
            // wraps around, only do the part inside
            pieceSize = vramSize - addr;
        else
            // fits completely inside
            pieceSize = size;

        hash = XXH64(&vram[addr], pieceSize, hash);

        addr += pieceSize;
        addr &= (vramSize - 1);
        assert(size >= pieceSize);
        size -= pieceSize;
    }

    return hash;
}

bool TexcacheCheckInvalid(u32 start, u32 size, u64 oldHash, u64* dirty, u8* vram, u32 vramSize)
{
    u32 startBit = start / VRAMDirtyGranularity;
    u32 bitsCount = ((start + size + VRAMDirtyGranularity - 1) / VRAMDirtyGranularity) - startBit;

    u32 startEntry = startBit >> 6;
    u64 entriesCount = ((startBit + bitsCount + 0x3F) >> 6) - startEntry;
    for (u32 j = startEntry; j < startEntry + entriesCount; j++)
    {
        if (GetRangedBitMask(j, startBit, bitsCount) & dirty[j & ((vramSize / VRAMDirtyGranularity)-1)])
        {
            if (TexcacheMaskedHash(vram, vramSize, start, size) != oldHash)
                return true;
        }
    }

    return false;
}

}
