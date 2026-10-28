#include "OrasFlyMap.h"

#include "Bclim.h"
#include "Darc.h"
#include "PicaTexture.h"

#include <cstring>
#include <string>

namespace remake
{

namespace
{
constexpr uint32_t Width = 380, Height = 270;
constexpr uint8_t PicaEtc1 = 0x0C; // PICA's ETC1 (CLIM's 10)

uint32_t Word(const Bytes& b, size_t at) { return (uint32_t)(b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24); }

// the layout with every picture inside the pane "s" made transparent: its sections in order, pas1/pae1 opening and closing
// a pane's children; returns how many pictures
Bytes WithPlacesTransparent(Bytes layout, size_t& pictures)
{
    if (layout.size() < 0x14 || std::memcmp(layout.data(), "CLYT", 4) != 0) throw FormatError("fly map: map_lower.bclyt is not a CLYT layout");
    size_t at = (uint16_t)(layout[6] | layout[7] << 8);
    std::string last;  // the last pane's name
    int depth = -1;    // the children's depth inside "s", -1 outside it
    pictures = 0;
    while (at + 8 <= layout.size())
    {
        const std::string magic(layout.begin() + (long)at, layout.begin() + (long)at + 4);
        const uint32_t size = Word(layout, at + 4);
        if (size < 8 || at + size > layout.size()) throw FormatError("fly map: a layout section past the file");
        const bool pane = magic == "pan1" || magic == "pic1" || magic == "bnd1" || magic == "txt1" || magic == "wnd1";
        if (pane)
        {
            last = std::string(reinterpret_cast<const char*>(&layout[at + 12]), strnlen(reinterpret_cast<const char*>(&layout[at + 12]), 16));
            if (depth > 0 && magic == "pic1") { layout[at + 10] = 0; pictures++; }
        }
        else if (magic == "pas1")
        {
            if (depth >= 0) depth++;
            else if (last == "s") depth = 1;
        }
        else if (magic == "pae1" && depth >= 0) depth--;
        if (depth == 0) depth = -1;
        at += size;
    }
    if (!pictures) throw FormatError("fly map: no picture under the pane s (another layout?)");
    return layout;
}
}

Bytes FlyMapWithBackground(const Bytes& member, const Bytes& rgba)
{
    if (rgba.size() != (size_t)Width * Height * 4) throw FormatError("fly map: the background is 380 x 270 RGBA");
    size_t darcAt = 0;
    for (; darcAt + 4 <= member.size() && std::memcmp(&member[darcAt], "darc", 4) != 0; darcAt += 4) {}
    if (darcAt + 4 > member.size()) throw FormatError("fly map: no DARC in a/0/6/7 member 0");
    std::vector<DarcFile> files = ReadDarc(Bytes(member.begin() + (long)darcAt, member.end()));
    bool background = false, layout = false;
    for (DarcFile& f : files)
    {
        if (f.Path.find("poke_navi_map.bclim") != std::string::npos)
        {
            const ClimImage old = ClimImage::Read(f.Data);
            if (old.Width != Width || old.Height != Height || old.Format != 10) throw FormatError("fly map: poke_navi_map.bclim is not 380 x 270 ETC1");
            // the image in the stored size's bottom-left corner, upside down: PicaTextureDecode gives the game's Hoenn there
            // (rows 242-511 of the 512 x 512, the rest a filler) and upside down, as the layout's picture samples it; an
            // image put in the top-left corner, where ClimImage::Rgba crops, showed black (run fm4), one put the right way up
            // showed upside down (run fm5, 28 October); the file's footer kept
            Bytes padded((size_t)old.StoredWidth * old.StoredHeight * 4, 0);
            for (uint32_t y = 0; y < Height; y++)
                std::copy(rgba.begin() + (long)(y * Width * 4), rgba.begin() + (long)((y + 1) * Width * 4),
                          padded.begin() + (long)((size_t)(old.StoredHeight - 1 - y) * old.StoredWidth * 4));
            const Bytes pixels = PicaTextureEncode(padded, old.StoredWidth, old.StoredHeight, PicaEtc1);
            if (pixels.size() != old.Pixels.size()) throw FormatError("fly map: the encoded background's size differs from the game's");
            std::copy(pixels.begin(), pixels.end(), f.Data.begin());
            background = true;
        }
        else if (f.Path.find("map_lower.bclyt") != std::string::npos)
        {
            size_t pictures = 0;
            f.Data = WithPlacesTransparent(f.Data, pictures);
            layout = true;
        }
    }
    if (!background || !layout) throw FormatError("fly map: poke_navi_map.bclim or map_lower.bclyt not in the DARC");
    const Bytes darc = WriteDarc(files);
    if (darc.size() != member.size() - darcAt) throw FormatError("fly map: the rewritten DARC's size differs (the header before it would need its offsets)");
    Bytes out(member.begin(), member.begin() + (long)darcAt);
    out.insert(out.end(), darc.begin(), darc.end());
    return out;
}

}
