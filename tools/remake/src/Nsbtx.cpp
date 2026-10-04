#include "Nsbtx.h"

namespace remake
{

long Tex0::Find(const Bytes& file)
{
    if (file.size() < 16) return -1;
    const std::string magic = Text(file, 0, 4);
    if (magic != "BTX0" && magic != "BMD0") return -1;
    const uint16_t blocks = U16(file, 14);
    for (uint16_t i = 0; i < blocks; i++)
    {
        const uint32_t at = U32(file, 16 + i * 4);
        if (Text(file, at, 4) == "TEX0") return (long)at;
    }
    return -1;
}

Tex0::Tex0(const Bytes& file, size_t at) : File(file)
{
    if (Text(File, at, 4) != "TEX0") throw FormatError("not a TEX0 block");
    TexData = at + U32(File, at + 0x14);
    CompData = at + U32(File, at + 0x24);
    CompInfo = at + U32(File, at + 0x28);
    PalData = at + U32(File, at + 0x38);
    for (const DictEntry& e : ReadDictionary(File, at + U16(File, at + 0x0E)))
    {
        Tex0Texture t;
        t.Name = e.Name;
        t.Param = U32(File, e.Data);
        t.Format.Format = (t.Param >> 26) & 7;
        t.Format.Width = 8u << ((t.Param >> 20) & 7);
        t.Format.Height = 8u << ((t.Param >> 23) & 7);
        t.Format.Colour0Transparent = (t.Param >> 29) & 1;
        TextureList.push_back(t);
    }
    for (const DictEntry& e : ReadDictionary(File, at + U32(File, at + 0x34)))
    {
        PaletteNames.push_back(e.Name);
        PaletteOffsets.push_back((size_t)U16(File, e.Data) << 3);
    }
}

int Tex0::DefaultPalette(size_t texture) const
{
    const std::string want = TextureList.at(texture).Name + "_pl";
    for (size_t i = 0; i < PaletteNames.size(); i++)
        if (PaletteNames[i] == want) return (int)i;
    return PaletteNames.empty() ? -1 : 0;
}

Bytes Tex0::Decode(size_t texture, int palette) const
{
    const Tex0Texture& t = TextureList.at(texture);
    const size_t offset = (size_t)(t.Param & 0xFFFF) << 3;
    const size_t texels = (size_t)t.Format.Width * t.Format.Height;
    const int bpp[8] = {0, 8, 2, 4, 8, 2, 8, 16};
    const int f = t.Format.Format;
    if (f == 0) throw FormatError("texture " + t.Name + ": no format");
    const size_t bytes = texels * bpp[f] / 8;
    const Bytes data = Slice(File, (f == 5 ? CompData : TexData) + offset, bytes);
    Bytes info;
    if (f == 5) info = Slice(File, CompInfo + offset / 2, texels / 16 * 2);
    if (palette < 0) palette = DefaultPalette(texture);
    Bytes pal;
    if (f != 7)
    {
        if (palette < 0) throw FormatError("texture " + t.Name + ": no palette");
        const size_t colours = f == 2 ? 4 : f == 3 ? 16 : f == 1 ? 32 : f == 6 ? 8 : 256;
        // the 4x4 format addresses its palette per block: take what the file has from there
        const size_t start = PalData + PaletteOffsets.at(palette);
        pal = Slice(File, start, f == 5 ? std::min<size_t>(File.size() - start, 0x10000) : colours * 2);
    }
    return DecodeTexture(t.Format, data, pal, info);
}

}
