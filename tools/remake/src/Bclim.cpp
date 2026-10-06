#include "Bclim.h"

#include "PicaTexture.h"

namespace remake
{

static const uint8_t ToPica[14] = {7, 8, 9, 5, 6, 3, 1, 2, 4, 0, 12, 13, 10, 11};
static const char* const Names[14] = {"L8", "A8", "LA4", "LA8", "HiLo8", "RGB565", "RGB8", "RGBA5551", "RGBA4", "RGBA8", "ETC1", "ETC1A4", "L4", "A4"};

const char* ClimFormatName(uint32_t format) { return format < 14 ? Names[format] : "?"; }

static uint32_t Pow2(uint32_t v) { uint32_t p = 8; while (p < v) p <<= 1; return p; }

ClimImage ClimImage::Read(const Bytes& f)
{
    if (f.size() < 0x28) throw FormatError("BCLIM: too short");
    const size_t footer = f.size() - 0x28;
    if (Text(f, footer, 4) != "CLIM" || Text(f, footer + 0x14, 4) != "imag") throw FormatError("BCLIM: no CLIM/imag footer");
    ClimImage c;
    c.Width = U16(f, footer + 0x1C);
    c.Height = U16(f, footer + 0x1E);
    c.Format = U32(f, footer + 0x20);
    if (c.Format >= 14) throw FormatError("BCLIM: format " + std::to_string(c.Format));
    const uint32_t dataSize = U32(f, footer + 0x24);
    if (dataSize > footer) throw FormatError("BCLIM: data size past the file");
    c.StoredWidth = c.Width; c.StoredHeight = c.Height;
    if (PicaTextureLength(c.Width, c.Height, ToPica[c.Format]) != dataSize) { c.StoredWidth = Pow2(c.Width); c.StoredHeight = Pow2(c.Height); }
    if (PicaTextureLength(c.StoredWidth, c.StoredHeight, ToPica[c.Format]) != dataSize) throw FormatError("BCLIM: data size matches neither the image's size nor its power-of-two rounding");
    c.Pixels = Slice(f, 0, dataSize);
    return c;
}

Bytes ClimImage::Rgba() const
{
    const Bytes all = PicaTextureDecode(Pixels, StoredWidth, StoredHeight, ToPica[Format]);
    Bytes out;
    for (uint32_t y = 0; y < Height; y++) out.insert(out.end(), all.begin() + (size_t)y * StoredWidth * 4, all.begin() + ((size_t)y * StoredWidth + Width) * 4);
    return out;
}

Bytes ClimImage::WriteRgba8(uint16_t width, uint16_t height, const Bytes& rgba)
{
    if (rgba.size() != (size_t)width * height * 4) throw FormatError("BCLIM: RGBA size does not match the image");
    const uint32_t sw = Pow2(width), sh = Pow2(height);
    Bytes padded((size_t)sw * sh * 4, 0);
    for (uint32_t y = 0; y < height; y++) std::copy(rgba.begin() + (size_t)y * width * 4, rgba.begin() + (size_t)(y + 1) * width * 4, padded.begin() + (size_t)y * sw * 4);
    Bytes out = PicaTextureEncodeRgba8(padded, sw, sh);
    const uint32_t dataSize = (uint32_t)out.size(), fileSize = dataSize + 0x28;
    auto p16 = [&](uint16_t v) { out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8)); };
    auto p32 = [&](uint32_t v) { for (int k = 0; k < 4; k++) out.push_back((uint8_t)(v >> (8 * k))); };
    for (char ch : std::string("CLIM")) out.push_back((uint8_t)ch);
    p16(0xFEFF); p16(0x14); p32(0x02020000); p32(fileSize); p16(1); p16(0);
    for (char ch : std::string("imag")) out.push_back((uint8_t)ch);
    p32(0x10); p16(width); p16(height); p32(9); p32(dataSize);
    return out;
}

}
