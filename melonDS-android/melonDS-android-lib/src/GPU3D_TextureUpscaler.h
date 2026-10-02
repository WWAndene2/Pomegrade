#ifndef GPU3D_TEXTUREUPSCALER_H
#define GPU3D_TEXTUREUPSCALER_H

// Native texture upscaling (Pomegrade): each DS texture is magnified x2 or x4
// when the game loads it, at the same size on screen. No AI: MMPX, a set of
// pixel-art rules that only copies existing texel colours (no blur, no new
// colours), so outlines and palettes are kept.
//
// MMPX: Morgan McGuire & Mara Gagiu, "MMPX Style-Preserving Pixel Art
// Magnification", Journal of Graphics Techniques 10(2), 2021. Reference code
// Copyright 2020 Morgan McGuire & Mara Gagiu, MIT license
// (https://casual-effects.com/research/McGuire2021PixelArt/).

#include "types.h"

#include <vector>

namespace melonDS
{

class TextureUpscaler
{
public:
    // How the texture continues past its edges, as the DS samples it.
    enum class Edge { Clamp, Repeat, Mirror };

    // RGBA8 texels (r | g<<8 | b<<16 | a<<24), w*h -> 2w*2h.
    static void MMPX2x(const u32* src, u32 width, u32 height, Edge edgeS, Edge edgeT, u32* dst);

    // Decoded DS texture (RGB6A5, one u32 per texel, as the texture cache
    // decodes it) to an RGBA8 texture factor (2 or 4) times larger. With
    // binaryAlpha, texels are either opaque or fully transparent.
    static void Upscale(const u32* decoded, u32 width, u32 height, int factor, bool binaryAlpha,
                        Edge edgeS, Edge edgeT, std::vector<u32>& out);

    // The DS texture parameter's repeat/flip bits for one axis (0 = S, 1 = T).
    static Edge EdgeFromTexParam(u32 texParam, int axis);
};

}

#endif
