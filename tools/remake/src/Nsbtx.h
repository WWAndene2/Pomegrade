#ifndef REMAKE_NSBTX_H
#define REMAKE_NSBTX_H

// The textures and palettes of a TEX0 block (an NSBTX file, or the one an
// NSBMD may carry), decoded to RGBA8. Each texture's dictionary entry is its
// TEXIMAGE_PARAM (offset in 8-byte units, size, format, colour 0 transparent);
// each palette's is its offset in 8-byte units. Which palette goes with
// which texture is the model's choice (its materials); without a model, a
// texture is paired with the palette named "<texture>_pl", else the first.
// Layout from the community documentation; to be checked on a real file.

#include "NitroDictionary.h"
#include "NitroTexture.h"

#include <string>
#include <vector>

namespace remake
{

struct Tex0Texture
{
    std::string Name;
    TextureFormat Format;
    uint32_t Param = 0;
};

// a texture's bytes as the game uploads them to VRAM
struct Tex0Raw
{
    TextureFormat Format;
    Bytes Texels, Palette, BlockInfo;
};

class Tex0
{
public:
    // file: the whole NSBTX/NSBMD; at: the TEX0 block's offset in it
    Tex0(const Bytes& file, size_t at);
    // the TEX0 block of a BTX0 or BMD0 file, or -1
    static long Find(const Bytes& file);

    const std::vector<Tex0Texture>& Textures() const { return TextureList; }
    const std::vector<std::string>& Palettes() const { return PaletteNames; }
    // RGBA8; palette: an index into Palettes(), -1: the default pairing
    Bytes Decode(size_t texture, int palette = -1) const;
    Tex0Raw Raw(size_t texture, int palette = -1) const;
    int DefaultPalette(size_t texture) const;

private:
    Bytes File;
    size_t TexData = 0, CompData = 0, CompInfo = 0, PalData = 0;
    std::vector<Tex0Texture> TextureList;
    std::vector<std::string> PaletteNames;
    std::vector<size_t> PaletteOffsets;
};

}

#endif // REMAKE_NSBTX_H
