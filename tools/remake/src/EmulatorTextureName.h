#ifndef REMAKE_EMULATORTEXTURENAME_H
#define REMAKE_EMULATORTEXTURENAME_H

// The name Pomegrade's DS core gives a texture it dumps or replaces
// (tex_<w>x<h>_<hash>, GPU3D_TextureReplacement): XXH64 of the texture
// decoded as melonDS's texture cache decodes it (RGB6A5, GPU3D_Texcache.cpp),
// seeded with (width << 32) | height. Computing it from the ROM's files ties
// every texture seen in game, dumped by the emulator, to the file it comes
// from. The decoder below must stay bit-identical to GPU3D_Texcache.cpp,
// including the colour kept in transparent texels.

#include "NitroTexture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace remake
{

std::vector<uint32_t> DecodeRgb6a5(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo = {});
std::string EmulatorTextureName(const TextureFormat& f, const Bytes& texels, const Bytes& palette, const Bytes& blockInfo = {});

}

#endif // REMAKE_EMULATORTEXTURENAME_H
