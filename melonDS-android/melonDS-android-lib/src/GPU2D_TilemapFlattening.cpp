#include "GPU2D_TilemapFlattening.h"

#include "xxhash/xxhash.h"

#include <set>

namespace melonDS
{

bool IsTextLayer(u32 dispCnt, int bg, bool engineB)
{
    const u32 mode = dispCnt & 7;
    if (bg == 0) return engineB || !(dispCnt & 0x8);
    if (bg == 1) return true;
    // modes 6-7 (engine A only) are large bitmaps and invalid
    if (bg == 2) return mode == 0 || mode == 1 || mode == 3;
    return mode == 0;
}

u32 ExtPaletteSlot(int bg, u16 bgCnt)
{
    return (bg < 2 && (bgCnt & 0x2000)) ? 2 + bg : bg;
}

// addresses as GPU2D_Soft.cpp's DrawBG_Text computes them
static void LayerBases(const TextLayerState& s, u32& tileset, u32& tilemap)
{
    tileset = (s.BgCnt & 0x003C) << 12;
    tilemap = (s.BgCnt & 0x1F00) << 3;
    if (!s.EngineB)
    {
        tileset += (s.DispCnt & 0x07000000) >> 8;
        tilemap += (s.DispCnt & 0x38000000) >> 11;
    }
}

static bool UsesExtPalette(const TextLayerState& s)
{
    return (s.BgCnt & 0x0080) && (s.DispCnt & 0x40000000) && s.ExtPalette;
}

FlatLayer FlattenTextLayer(const TextLayerState& s)
{
    FlatLayer out;
    out.Width = (s.BgCnt & 0x4000) ? 512 : 256;
    out.Height = (s.BgCnt & 0x8000) ? 512 : 256;
    out.Tiles = (out.Width / 8) * (out.Height / 8);
    out.ExtendedPalette = UsesExtPalette(s);
    out.Pixels.assign(out.Width * out.Height, 0);

    u32 tileset, tilemap;
    LayerBases(s, tileset, tilemap);
    const bool colours256 = s.BgCnt & 0x0080;
    std::set<u32> distinct;

    for (u32 ty = 0; ty < out.Height / 8; ty++)
    for (u32 tx = 0; tx < out.Width / 8; tx++)
    {
        // 32x32-entry screen blocks: right of x 256 the next one, below y 256
        // the next one (two on with a 512-wide map)
        u32 entry = tilemap + ((ty & 31) << 6) + ((tx & 31) << 1);
        if (tx >= 32) entry += 0x800;
        if (ty >= 32) entry += out.Width == 512 ? 0x1000 : 0x800;
        const u16 tile = s.Vram[entry & s.VramMask] | (s.Vram[(entry + 1) & s.VramMask] << 8);

        distinct.insert(tile & 0x3FF);
        if (tile & 0x0C00) out.FlippedTiles++;
        const bool hflip = tile & 0x0400, vflip = tile & 0x0800;
        const u32 pal = tile >> 12;

        for (u32 py = 0; py < 8; py++)
        for (u32 px = 0; px < 8; px++)
        {
            const u32 sx = hflip ? 7 - px : px, sy = vflip ? 7 - py : py;
            u16 pixel = 0;
            if (colours256)
            {
                const u8 c = s.Vram[(tileset + ((tile & 0x3FF) << 6) + (sy << 3) + sx) & s.VramMask];
                if (c) pixel = 0x8000 | (out.ExtendedPalette ? pal << 8 : 0) | c;
            }
            else
            {
                const u8 b = s.Vram[(tileset + ((tile & 0x3FF) << 5) + (sy << 2) + (sx >> 1)) & s.VramMask];
                const u8 c = (sx & 1) ? b >> 4 : b & 0xF;
                if (c) pixel = 0x8000 | (pal << 4) | c;
            }
            out.Pixels[(ty * 8 + py) * out.Width + tx * 8 + px] = pixel;
        }
    }

    out.DistinctTiles = distinct.size();
    out.Hash = XXH32(out.Pixels.data(), out.Pixels.size() * sizeof(u16), 0);
    return out;
}

std::vector<u16> FlatLayerColours(const FlatLayer& layer, const TextLayerState& s)
{
    std::vector<u16> out(layer.Pixels.size(), 0);
    for (size_t i = 0; i < out.size(); i++)
    {
        const u16 p = layer.Pixels[i];
        if (!p) continue;
        const u16* pal = layer.ExtendedPalette ? s.ExtPalette((p >> 8) & 0xF) : s.Palette;
        out[i] = (pal[p & 0xFF] & 0x7FFF) | 0x8000;
    }
    return out;
}

}
