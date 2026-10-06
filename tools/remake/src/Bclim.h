#ifndef REMAKE_BCLIM_H
#define REMAKE_BCLIM_H

// BCLIM: a 3DS layout image (the title logo, menus). The pixels first, in PICA200 tiles (PicaTexture.h), then a 0x28-byte
// footer: "CLIM" header (u16 BOM 0xFEFF, u16 header size 0x14, u32 version, u32 file size, u16 block count, u16 0), an
// "imag" block (u32 size 0x10, u16 width, u16 height, u32 format), u32 pixel data size. The stored size is the image's
// rounded up to powers of two (at least 8) when the data size says so. Format numbers are CLIM's own (0 L8, 1 A8, 2 LA4,
// 3 LA8, 4 HiLo8, 5 RGB565, 6 RGB8, 7 RGBA5551, 8 RGBA4, 9 RGBA8, 10 ETC1, 11 ETC1A4, 12 L4, 13 A4), mapped to PICA's.
// The layout is the community's (Ohana3DS, Kuriimu); checked here against itself (remake_layout_test) and read
// on the game's title logos (run 125).

#include "Bytes.h"

namespace remake
{

struct ClimImage
{
    uint16_t Width = 0, Height = 0;   // the image's
    uint32_t StoredWidth = 0, StoredHeight = 0;
    uint32_t Format = 0;              // CLIM's numbering
    Bytes Pixels;                     // as stored

    static ClimImage Read(const Bytes& file);
    // RGBA rows top to bottom, Width x Height
    Bytes Rgba() const;
    // a BCLIM file of the image as RGBA8 (format 9), stored at the same rounded size
    static Bytes WriteRgba8(uint16_t width, uint16_t height, const Bytes& rgba);
};

const char* ClimFormatName(uint32_t format);

}

#endif // REMAKE_BCLIM_H
