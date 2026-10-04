#ifndef GPU2D_TILEMAPFLATTENING_H
#define GPU2D_TILEMAPFLATTENING_H

// Tilemap flattening (Pomegrade, DS_ENGINE_REMAKE.md 5.5, 5.12 step 12): a
// text background is a map of 8x8 tiles, so an upscaler working on what the
// hardware draws sees each tile alone and leaves seams at tile borders. This
// stitches a whole text layer (256 or 512 pixels each way, as its BGCNT says)
// into one image from its tilemap, tile data and flips, the input a
// replacement or an upscaler needs to see the map with full context.
//
// The image holds palette indices, not colours: the palette is applied after
// stitching (FlatLayerColours), so a game animating its palette (water, lava,
// flashing text) changes no stitched image, only the colours given to it.
// Rotated and scaled (affine, extended, bitmap) layers are not text layers;
// re-rasterising them is not done. Per-scanline scroll changes are not seen:
// the layer is read with the registers of one moment.

#include "types.h"

#include <functional>
#include <vector>

namespace melonDS
{

// what drawing one 2D engine's text layer reads
struct TextLayerState
{
    u32 DispCnt = 0;
    u16 BgCnt = 0;
    bool EngineB = false;
    const u8* Vram = nullptr; // the engine's background VRAM (GPU2D::Unit::GetBGVRAM)
    u32 VramMask = 0;
    const u16* Palette = nullptr; // the engine's 256 background colours
    // palette 0-15 of this layer's extended palette slot (ExtPaletteSlot),
    // used by 256-colour layers when DISPCNT bit 30 is set
    std::function<const u16*(u32 palette)> ExtPalette;
};

struct FlatLayer
{
    u32 Width = 0, Height = 0;
    // per pixel: 0 transparent, else 0x8000 | extended palette << 8 | colour
    // index (into the 256 standard colours, or into that extended palette)
    std::vector<u16> Pixels;
    bool ExtendedPalette = false;
    u32 Tiles = 0;          // map entries (Width / 8 * Height / 8)
    u32 DistinctTiles = 0;  // different tile numbers used
    u32 FlippedTiles = 0;   // entries flipped horizontally or vertically
    u32 Hash = 0;           // of Pixels: what the layer draws, palette aside
};

// whether background 0-3 is a text layer in this DISPCNT (BG0 drawn by the 3D
// engine is not)
bool IsTextLayer(u32 dispCnt, int bg, bool engineB);
// the extended palette slot of background 0-3 (BG0 and BG1 may use 2 and 3)
u32 ExtPaletteSlot(int bg, u16 bgCnt);
FlatLayer FlattenTextLayer(const TextLayerState& state);
// colours of a flattened layer: BGR555 | 0x8000, 0 where transparent
std::vector<u16> FlatLayerColours(const FlatLayer& layer, const TextLayerState& state);

}

#endif // GPU2D_TILEMAPFLATTENING_H
