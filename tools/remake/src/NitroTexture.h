#ifndef REMAKE_NITROTEXTURE_H
#define REMAKE_NITROTEXTURE_H

// The DS 3D engine's seven texture formats, decoded to RGBA8 as the hardware
// draws them (the same rules as melonDS's GPU3D_Texcache):
// 1 A3I5 (8 bits: 5 palette index, 3 alpha), 2 four colours (2 bits),
// 3 sixteen colours (4 bits), 4 256 colours (8 bits), 5 4x4-texel
// compressed (2 bits a texel, a palette index and mode per block),
// 6 A5I3 (3 index, 5 alpha), 7 direct colour (16 bits, BGR555 + alpha bit).

#include "Bytes.h"

namespace remake
{

struct TextureFormat
{
    int Format = 0;          // 1-7
    uint32_t Width = 0, Height = 0;
    bool Colour0Transparent = false; // formats 2-4: palette entry 0 is transparent
};

// texels: the texture's data; palette: its palette's colours (BGR555, u16 each;
// unused by format 7); blockInfo: format 5's per-block palette words (u16 per
// 4x4 block, in block order). Returns width * height * 4 RGBA bytes
Bytes DecodeTexture(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo = {});
// a BGR555 colour widened to RGB8 as the DS does (c << 3 | c >> 2)
void Bgr555ToRgb(uint16_t c, uint8_t* rgb);

}

#endif // REMAKE_NITROTEXTURE_H
