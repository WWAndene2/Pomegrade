#ifndef REMAKE_TEXTUREINDEX_H
#define REMAKE_TEXTUREINDEX_H

// Every texture of a DS cartridge under the name the emulator dumps it with
// (EmulatorTextureName), so a dump folder made while playing says which ROM
// files a scene uses. A texture's palette is chosen at run time by the
// model, so each texture is indexed with every palette of its file; for the
// palette formats (2-4) both values of "colour 0 transparent" are indexed,
// since the game's material may set the bit the file's texture lacks.

#include "NdsRom.h"

#include <string>
#include <vector>

namespace remake
{

struct TextureSource
{
    std::string Name;     // tex_<w>x<h>_<hash>
    std::string Path;     // ROM path, "#<member>" inside a NARC
    std::string Texture, Palette;
    int Format = 0;
};

std::vector<TextureSource> IndexTextures(const NdsRom& rom);
// the TEX0 textures of one (decompressed) file, path as given
void IndexTextures(const Bytes& file, const std::string& path, std::vector<TextureSource>& out);

}

#endif // REMAKE_TEXTUREINDEX_H
