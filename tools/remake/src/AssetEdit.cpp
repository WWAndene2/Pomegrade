#include "AssetEdit.h"

#include <algorithm>
#include <cmath>
#include "Bch.h"
#include "PicaTexture.h"

namespace remake
{

Bytes BchEditTexture(const Bytes& bch, const std::string& name, const TextureEditFn& edit)
{
    for (const BchTexture& t : Bch::Read(bch).Textures)
    {
        if (t.Name != name || t.Data.empty()) continue;
        Bytes rgba = PicaTextureDecode(t.Data, t.Width, t.Height, t.Format);
        edit(rgba, t.Width, t.Height);
        const Bytes encoded = PicaTextureEncode(rgba, t.Width, t.Height, t.Format);
        // the texture's own bytes only: PicaTextureLength pads to 0x80, the file's data may not
        const size_t length = std::min(encoded.size(), t.Data.size());
        if (t.DataOffset + length > bch.size()) throw FormatError("BCH: texture " + name + " lies past the file");
        Bytes out = bch;
        std::copy(encoded.begin(), encoded.begin() + (ptrdiff_t)length, out.begin() + (ptrdiff_t)t.DataOffset);
        return out;
    }
    throw FormatError("BCH: no texture named " + name);
}

static void ToHsv(double r, double g, double b, double& h, double& s, double& v)
{
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx;
    s = mx > 0 ? d / mx : 0;
    if (d == 0) h = 0;
    else if (mx == r) h = 60 * std::fmod((g - b) / d + 6, 6);
    else if (mx == g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
}

static void FromHsv(double h, double s, double v, double& r, double& g, double& b)
{
    const double c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60, 2) - 1)), m = v - c;
    const int sector = (int)(h / 60) % 6;
    const double rs[6] = {c, x, 0, 0, x, c}, gs[6] = {x, c, c, x, 0, 0}, bs[6] = {0, 0, x, c, c, x};
    r = rs[sector] + m; g = gs[sector] + m; b = bs[sector] + m;
}

void Recolour(Bytes& rgba, double hue, double saturation, double brightness, double around, double range)
{
    for (size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        double h, s, v;
        ToHsv(rgba[i] / 255.0, rgba[i + 1] / 255.0, rgba[i + 2] / 255.0, h, s, v);
        if (range < 180)
        {
            // a grey has no hue: left alone when only one colour of the model changes
            const double off = std::fabs(std::fmod(h - around + 540, 360) - 180);
            if (s < 0.08 || off > range) continue;
        }
        h = std::fmod(h + hue + 360 * 4, 360);
        s = std::clamp(s * saturation, 0.0, 1.0);
        v = std::clamp(v * brightness, 0.0, 1.0);
        double r, g, b;
        FromHsv(h, s, v, r, g, b);
        rgba[i] = (uint8_t)std::lround(r * 255);
        rgba[i + 1] = (uint8_t)std::lround(g * 255);
        rgba[i + 2] = (uint8_t)std::lround(b * 255);
    }
}

Bytes Resample(const Bytes& rgba, uint32_t fw, uint32_t fh, uint32_t w, uint32_t h)
{
    if (fw == w && fh == h) return rgba;
    Bytes out((size_t)w * h * 4);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
        {
            // pixel centres mapped onto the source, edges clamped
            const double sx = std::clamp((x + 0.5) * fw / w - 0.5, 0.0, fw - 1.0), sy = std::clamp((y + 0.5) * fh / h - 0.5, 0.0, fh - 1.0);
            const uint32_t x0 = (uint32_t)sx, y0 = (uint32_t)sy, x1 = std::min(x0 + 1, fw - 1), y1 = std::min(y0 + 1, fh - 1);
            const double fx = sx - x0, fy = sy - y0;
            for (int c = 0; c < 4; c++)
            {
                auto at = [&](uint32_t px, uint32_t py) { return (double)rgba[((size_t)py * fw + px) * 4 + c]; };
                const double top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx, bottom = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
                out[((size_t)y * w + x) * 4 + c] = (uint8_t)std::lround(top * (1 - fy) + bottom * fy);
            }
        }
    return out;
}

}
