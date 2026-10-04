#include "GPU3D_MaterialClassifier.h"

#include "GPU.h"
#include "GPU3D_Texcache.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace melonDS
{

const char* MaterialClassifier::Name(MaterialClass c) noexcept
{
    static const char* names[] = {"unknown", "water", "lava", "foliage", "wood", "stone", "sky"};
    return names[(int)c];
}

const MaterialClassifier::TextureStats& MaterialClassifier::Statistics(GPU& gpu, u32 texParam, u32 palette)
{
    // address, size and format, and the palette: what the texels are
    const u64 key = ((u64)palette << 32) | (texParam & 0x1FFFFFFF & ~(3u << 16) & ~(3u << 18));
    auto found = Stats.find(key);
    if (found != Stats.end()) return found->second;

    const u32 w = TextureWidth(texParam), h = TextureHeight(texParam);
    std::vector<u32> texels(w * h);
    TexSource source;
    DecodeTexture(gpu, texParam, palette, texels.data(), source);

    TextureStats s;
    double r = 0, g = 0, b = 0, dx = 0, dy = 0;
    u32 opaque = 0, pairsX = 0, pairsY = 0;
    auto lum = [](u32 c) { return ((c & 0x3F) * 2 + ((c >> 8) & 0x3F) * 5 + ((c >> 16) & 0x3F)) / (8.0 * 63); };
    for (u32 y = 0; y < h; y++)
        for (u32 x = 0; x < w; x++)
        {
            const u32 c = texels[y * w + x];
            if (!(c >> 24)) continue;
            opaque++;
            r += (c & 0x3F) / 63.0; g += ((c >> 8) & 0x3F) / 63.0; b += ((c >> 16) & 0x3F) / 63.0;
            // differences with the next texel across and down (wrapping: tiles)
            const u32 cx = texels[y * w + (x + 1) % w], cy = texels[((y + 1) % h) * w + x];
            if (cx >> 24) { dx += std::fabs(lum(c) - lum(cx)); pairsX++; }
            if (cy >> 24) { dy += std::fabs(lum(c) - lum(cy)); pairsY++; }
        }
    s.Transparent = 1.0f - (float)opaque / (w * h);
    if (opaque)
    {
        r /= opaque; g /= opaque; b /= opaque;
        const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
        s.Brightness = (float)mx;
        s.Saturation = mx > 0 ? (float)((mx - mn) / mx) : 0;
        double hue = 0;
        if (mx > mn)
        {
            if (mx == r) hue = 60 * std::fmod((g - b) / (mx - mn) + 6, 6.0);
            else if (mx == g) hue = 60 * ((b - r) / (mx - mn) + 2);
            else hue = 60 * ((r - g) / (mx - mn) + 4);
        }
        s.Hue = (float)hue;
        double spread = 0;
        for (u32 c : texels)
            if (c >> 24)
                spread += (std::fabs((c & 0x3F) / 63.0 - r) + std::fabs(((c >> 8) & 0x3F) / 63.0 - g) + std::fabs(((c >> 16) & 0x3F) / 63.0 - b)) / 3;
        s.Variety = (float)(spread / opaque);
        const double ex = pairsX ? dx / pairsX : 0, ey = pairsY ? dy / pairsY : 0;
        s.Detail = (float)((ex + ey) / 2);
        // grain: texels change across the grain, not along it
        s.Grain = ex + ey > 0 ? (float)(std::fabs(ex - ey) / (ex + ey)) : 0;
    }
    return Stats.emplace(key, s).first->second;
}

MaterialResult MaterialClassifier::Classify(GPU& gpu, u32 texParam, u32 palette, const TextureEvidence& e)
{
    const TextureStats& s = Statistics(gpu, texParam, palette);
    float votes[(int)MaterialClass::Count] = {};
    MaterialResult result;
    auto vote = [&](MaterialClass c, float weight, const char* cue) {
        votes[(int)c] += weight;
        if (!result.Cues.empty()) result.Cues += ", ";
        result.Cues += cue;
    };
    constexpr float RenderState = 0.6f, TextureStats = 0.3f;

    // render state
    const u32 format = (texParam >> 26) & 7;
    const bool cutout = s.Transparent > 0.05f && s.Transparent < 0.95f &&
                        (format == 7 || ((texParam >> 29) & 1) || format == 1 || format == 6);
    if (e.Polygons && e.Translucent * 2 > e.Polygons) vote(MaterialClass::Water, RenderState * 0.5f, "translucent");
    if (e.Scrolling)
    {
        // scrolling: water, or lava when the texture is red
        const bool warm = s.Saturation > 0.3f && (s.Hue < 50 || s.Hue > 340);
        vote(warm ? MaterialClass::Lava : MaterialClass::Water, RenderState, warm ? "scrolling, warm" : "scrolling");
    }
    // texture statistics. A texture of many colours (a character's atlas)
    // has no one hue to go by
    const bool coloured = s.Saturation > 0.25f && s.Variety < 0.15f;
    const bool green = coloured && s.Hue >= 70 && s.Hue < 170;
    if (cutout && green) vote(MaterialClass::Foliage, RenderState * 0.5f, "cut-out alpha, green");
    if (coloured && s.Hue >= 170 && s.Hue <= 250)
    {
        // sky: bright (clouds); water: deep blue; grey-blue (metal) neither
        if (s.Brightness > 0.85f && s.Detail < 0.04f) vote(MaterialClass::Sky, TextureStats, "blue, bright, smooth");
        else if (s.Saturation > 0.5f && s.Brightness <= 0.85f) vote(MaterialClass::Water, TextureStats, "deep blue");
    }
    if (green && s.Detail > 0.04f) vote(MaterialClass::Foliage, TextureStats, "green, busy");
    if (coloured && s.Hue >= 15 && s.Hue < 50 && s.Saturation > 0.5f && s.Brightness < 0.85f)
        vote(MaterialClass::Wood, s.Grain > 0.3f ? TextureStats * 1.5f : TextureStats, s.Grain > 0.3f ? "brown, grain" : "brown");
    // stone: an opaque surface of dull, isotropic noise
    if (s.Transparent < 0.05f && s.Saturation < 0.4f && s.Detail > 0.02f && s.Grain < 0.3f)
        vote(MaterialClass::Stone, TextureStats, "dull, isotropic noise");
    if (coloured && (s.Hue < 15 || s.Hue > 340) && s.Brightness > 0.6f) vote(MaterialClass::Lava, TextureStats * 0.5f, "red, bright");

    // fusion: the best class, its confidence its share of the votes
    float total = 0, best = 0;
    for (int c = 1; c < (int)MaterialClass::Count; c++)
    {
        total += votes[c];
        if (votes[c] > best) { best = votes[c]; result.Class = (MaterialClass)c; }
    }
    // a lone weak cue is not enough: confidence grows with the evidence
    result.Confidence = total > 0 ? best / total * std::min(1.0f, best / 0.45f) : 0;
    if (result.Confidence < MinConfidence) result.Class = MaterialClass::Unknown;
    return result;
}

}
