#include "TextureIndex.h"

#include "EmulatorTextureName.h"
#include "Narc.h"
#include "NitroCompression.h"
#include "Nsbtx.h"

namespace remake
{

static Bytes Plain(const Bytes& d)
{
    if (!IsLzCompressed(d)) return d;
    try { return LzDecompress(d); }
    catch (const FormatError&) { return d; } // looked compressed, was not
}

void IndexTextures(const Bytes& file, const std::string& path, std::vector<TextureSource>& out)
{
    const long at = Tex0::Find(file);
    if (at < 0) return;
    try
    {
        const Tex0 tex(file, (size_t)at);
        for (size_t t = 0; t < tex.Textures().size(); t++)
        {
            const int format = tex.Textures()[t].Format.Format;
            const int palettes = format == 7 ? 1 : (int)tex.Palettes().size();
            for (int p = 0; p < palettes; p++)
            {
                Tex0Raw raw;
                try { raw = tex.Raw(t, format == 7 ? -1 : p); }
                catch (const FormatError&) { continue; } // palette too short for this format
                for (int transparent = 0; transparent < (format >= 2 && format <= 4 ? 2 : 1); transparent++)
                {
                    if (format >= 2 && format <= 4) raw.Format.Colour0Transparent = transparent;
                    TextureSource s;
                    s.Name = EmulatorTextureName(raw.Format, raw.Texels, raw.Palette, raw.BlockInfo);
                    s.Path = path;
                    s.Texture = tex.Textures()[t].Name;
                    s.Palette = format == 7 ? "" : tex.Palettes()[p];
                    s.Format = format;
                    out.push_back(std::move(s));
                }
            }
        }
    }
    catch (const FormatError&) {} // not the TEX0 layout this reader knows
}

std::vector<TextureSource> IndexTextures(const NdsRom& rom)
{
    std::vector<TextureSource> out;
    for (const NdsFile& f : rom.Files())
    {
        const Bytes data = Plain(rom.Read(f));
        if (Narc::Is(data))
        {
            try
            {
                const Narc narc(data);
                for (size_t i = 0; i < narc.Count(); i++)
                    IndexTextures(Plain(narc.Member(i)), f.Path + "#" + std::to_string(i), out);
            }
            catch (const FormatError&) {}
        }
        else IndexTextures(data, f.Path, out);
    }
    return out;
}

}
