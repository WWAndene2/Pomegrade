#ifndef REMAKE_PICATEXTURE_H
#define REMAKE_PICATEXTURE_H

// 3DS GPU (PICA200) textures: 14 formats (RGBA8, RGB8, RGBA5551, RGB565,
// RGBA4, LA8, HiLo8, L8, A8, LA4, L4, A4, ETC1, ETC1A4), stored in 8x8
// tiles (a fixed Z-order inside each), the bottom row first; ETC1 tiles are
// 4 blocks of 4x4 (ETC1A4: each preceded by 64 bits of 4-bit alpha). Sizes
// rounded up to 0x80 bytes. Layout from SPICA (public domain),
// TextureConverter and TextureCompression.

#include "Bytes.h"

namespace remake
{

size_t PicaTextureLength(uint32_t width, uint32_t height, uint8_t format);
// RGBA, rows top to bottom (as PNG and glTF expect)
Bytes PicaTextureDecode(const Bytes& data, uint32_t width, uint32_t height, uint8_t format);
// the reverse for format 0 (RGBA8): RGBA rows top to bottom in, the stored bytes out (8x8 tiles, bottom row first, ABGR),
// padded to PicaTextureLength; PicaTextureDecode(PicaTextureEncodeRgba8(x), w, h, 0) == x
Bytes PicaTextureEncodeRgba8(const Bytes& rgba, uint32_t width, uint32_t height);
// the reverse of PicaTextureDecode for every format: RGBA rows top to bottom in, the stored bytes out, padded to
// PicaTextureLength. Formats with fewer bits round each channel to the nearest level (decoding gives back the same
// levels); ETC1 and ETC1A4 are compressed: each 4x4 block takes the base colours, tables and split (two halves side by
// side or one above the other, individual or differential colours) whose decoding is closest to the pixels, checked
// through the decoder itself. Used to rewrite a model's texture in its own format (asset customization)
Bytes PicaTextureEncode(const Bytes& rgba, uint32_t width, uint32_t height, uint8_t format);

}

#endif // REMAKE_PICATEXTURE_H
