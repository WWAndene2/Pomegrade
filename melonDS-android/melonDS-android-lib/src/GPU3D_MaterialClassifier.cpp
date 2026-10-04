#include "GPU3D_MaterialClassifier.h"

#include "GPU.h"
#include "GPU3D_Texcache.h"
#include "xxhash/xxhash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace melonDS
{

const char* MaterialClassifier::Name(MaterialClass c) noexcept
{
    static const char* names[] = {"unknown", "water", "lava", "foliage", "wood", "stone", "sky", "character"};
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
    s.Hash = XXH32(texels.data(), texels.size() * 4, 0);
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
        // chroma only (each channel minus the texel's mean): a stone's light
        // and dark spots are one colour, an atlas's skin and cloth are not
        const double m = (r + g + b) / 3;
        double spread = 0;
        for (u32 c : texels)
            if (c >> 24)
            {
                const double cr = (c & 0x3F) / 63.0, cg = ((c >> 8) & 0x3F) / 63.0, cb = ((c >> 16) & 0x3F) / 63.0;
                const double l = (cr + cg + cb) / 3;
                spread += (std::fabs(cr - l - (r - m)) + std::fabs(cg - l - (g - m)) + std::fabs(cb - l - (b - m))) / 3;
            }
        s.Variety = (float)(spread / opaque);
        const double ex = pairsX ? dx / pairsX : 0, ey = pairsY ? dy / pairsY : 0;
        s.Detail = (float)((ex + ey) / 2);
        // grain: texels change across the grain, not along it
        s.Grain = ex + ey > 0 ? (float)(std::fabs(ex - ey) / (ex + ey)) : 0;
    }
    return Stats.emplace(key, s).first->second;
}

std::vector<MaterialClassifier::Region> MaterialClassifier::Segment(GPU& gpu, u32 texParam, u32 palette) const
{
    // bits per texel by format: 4, 16 and 256 colours only
    static const int bits[8] = {0, 0, 2, 4, 8, 0, 0, 0};
    const u32 format = (texParam >> 26) & 7;
    if (!bits[format]) return {};
    const u32 entries = 1u << bits[format];
    const u32 w = 8u << ((texParam >> 20) & 7), h = 8u << ((texParam >> 23) & 7);
    const u32 addr = (texParam & 0xFFFF) << 3;
    const u32 palAddr = format == 2 ? palette << 3 : palette << 4;
    const bool colour0Transparent = (texParam >> 29) & 1;

    std::vector<u32> counts(entries, 0);
    for (u32 i = 0; i < w * h; i++)
    {
        const u32 bit = i * bits[format];
        const u8 byte = gpu.ReadVRAM_Texture<u8>(addr + (bit >> 3));
        counts[(byte >> (bit & 7)) & (entries - 1)]++;
    }
    if (colour0Transparent) counts[0] = 0;

    // the used entries' colours, as lightness and chroma
    struct Entry { u8 Index; u32 Count; float R, G, B, L; int Group; };
    std::vector<Entry> used;
    for (u32 i = 0; i < entries; i++)
    {
        if (!counts[i]) continue;
        const u16 c = gpu.ReadVRAM_TexPal<u16>(palAddr + i * 2);
        Entry e {(u8)i, counts[i], (c & 0x1F) / 31.0f, ((c >> 5) & 0x1F) / 31.0f, ((c >> 10) & 0x1F) / 31.0f, 0, (int)used.size()};
        e.L = (e.R + e.G + e.B) / 3;
        used.push_back(e);
    }
    auto distance = [](const Entry& a, const Entry& b) {
        // the colour scaled to its brightest channel counts fully (shading
        // a material scales its channels: dark red and red are one colour),
        // lightness a quarter
        const float ma = std::max({a.R, a.G, a.B, 1.0f / 31}), mb = std::max({b.R, b.G, b.B, 1.0f / 31});
        const float dr = a.R / ma - b.R / mb, dg = a.G / ma - b.G / mb, db = a.B / ma - b.B / mb;
        return std::sqrt(dr * dr + dg * dg + db * db) + 0.25f * std::fabs(a.L - b.L);
    };
    // centroid linkage: the two groups whose mean colours are closest merge,
    // until none are closer than RegionDistance (single linkage chains a
    // 256-colour palette's ramps from one hue to the next into one group)
    std::vector<Entry> centres = used; // a group's weighted mean, Count its texels
    std::vector<bool> alive(centres.size(), true);
    for (;;)
    {
        float best = RegionDistance;
        int a = -1, b = -1;
        for (size_t i = 0; i < centres.size(); i++)
            if (alive[i])
                for (size_t j = i + 1; j < centres.size(); j++)
                    if (alive[j])
                    {
                        const float d = distance(centres[i], centres[j]);
                        if (d < best) { best = d; a = (int)i; b = (int)j; }
                    }
        if (a < 0) break;
        Entry& A = centres[a];
        const Entry& B = centres[b];
        const float n = (float)A.Count + B.Count;
        A.R = (A.R * A.Count + B.R * B.Count) / n;
        A.G = (A.G * A.Count + B.G * B.Count) / n;
        A.B = (A.B * A.Count + B.B * B.Count) / n;
        A.L = (A.R + A.G + A.B) / 3;
        A.Count = (u32)n;
        alive[b] = false;
        for (Entry& e : used) if (e.Group == b) e.Group = a;
    }

    u32 total = 0;
    for (const Entry& e : used) total += e.Count;
    std::map<int, Region> groups;
    std::map<int, std::array<double, 4>> sums; // r, g, b, count
    for (const Entry& e : used)
    {
        Region& r = groups[e.Group];
        r.Indices.push_back(e.Index);
        auto& s = sums[e.Group];
        s[0] += e.R * e.Count; s[1] += e.G * e.Count; s[2] += e.B * e.Count; s[3] += e.Count;
    }
    std::vector<Region> out;
    for (auto& [g, r] : groups)
    {
        const auto& s = sums[g];
        const float R = (float)(s[0] / s[3]), G = (float)(s[1] / s[3]), B = (float)(s[2] / s[3]);
        r.Share = total ? (float)(s[3] / total) : 0;
        r.Colour = (u16)(std::lround(R * 31) | (std::lround(G * 31) << 5) | (std::lround(B * 31) << 10));
        // skin: an orange hue, moderately saturated, light (any complexion
        // painted in DS games' usual ramps)
        const float mx = std::max({R, G, B}), mn = std::min({R, G, B});
        const float sat = mx > 0 ? (mx - mn) / mx : 0;
        float hue = 0;
        if (mx > mn && mx == R) hue = 60 * std::fmod((G - B) / (mx - mn) + 6, 6.0f);
        else if (mx > mn && mx == G) hue = 60 * ((B - R) / (mx - mn) + 2);
        if (mx == R && hue >= 10 && hue <= 45 && sat >= 0.2f && sat <= 0.65f && mx >= 0.45f) r.Label = "skin";
        out.push_back(std::move(r));
    }
    std::sort(out.begin(), out.end(), [](const Region& a, const Region& b) { return a.Share > b.Share; });
    return out;
}

std::string MaterialClassifier::ParseManifest(const std::string& text, MaterialManifest& out)
{
    std::istringstream lines(text);
    std::string line, errors;
    while (std::getline(lines, line))
    {
        const std::string content = line.substr(0, line.find('#'));
        std::istringstream words(content);
        std::string hash, name;
        if (!(words >> hash)) continue; // blank or comment
        words >> name;
        char* end = nullptr;
        const unsigned long value = std::strtoul(hash.c_str(), &end, 16);
        int cls = -1;
        for (int c = 0; c < (int)MaterialClass::Count; c++)
            if (name == Name((MaterialClass)c)) cls = c;
        if (*end || hash.size() > 8 || cls < 0)
        {
            errors += line + "\n";
            continue;
        }
        out[(u32)value] = (MaterialClass)cls;
    }
    return errors;
}

MaterialResult MaterialClassifier::Classify(GPU& gpu, u32 texParam, u32 palette, const TextureEvidence& e)
{
    const TextureStats& s = Statistics(gpu, texParam, palette);
    auto named = Manifest.find(s.Hash);
    if (named != Manifest.end())
        return {named->second, 1.0f, "manifest"};
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
    const bool coloured = s.Saturation > 0.25f && s.Variety < 0.10f;
    const bool green = coloured && s.Hue >= 70 && s.Hue < 170;
    if (cutout && green) vote(MaterialClass::Foliage, RenderState * 0.5f, "cut-out alpha, green");
    if (coloured && s.Hue >= 170 && s.Hue <= 250)
    {
        // sky: bright (clouds); water: deep blue; grey-blue (metal) neither
        if (s.Brightness > 0.85f && s.Detail < 0.04f) vote(MaterialClass::Sky, TextureStats, "blue, bright, smooth");
        else if (s.Saturation > 0.5f && s.Brightness <= 0.85f) vote(MaterialClass::Water, TextureStats, "deep blue");
        // full emission: it lights itself, whatever the scene's lights (Joker's sky)
        if (e.Lit * 2 > e.Polygons && e.Emission >= 24 && s.Detail < 0.04f) vote(MaterialClass::Sky, RenderState, "blue, self-lit");
    }
    if (green && s.Detail > 0.04f) vote(MaterialClass::Foliage, TextureStats, "green, busy");
    if (coloured && s.Hue >= 15 && s.Hue < 50 && s.Saturation > 0.5f && s.Brightness < 0.85f)
        vote(MaterialClass::Wood, s.Grain > 0.3f ? TextureStats * 1.5f : TextureStats, s.Grain > 0.3f ? "brown, grain" : "brown");
    // stone: an opaque surface of dull, isotropic noise
    if (s.Transparent < 0.05f && s.Saturation < 0.4f && s.Detail > 0.02f && s.Grain < 0.3f)
        vote(MaterialClass::Stone, TextureStats, "dull, isotropic noise");
    if (coloured && (s.Hue < 15 || s.Hue > 340) && s.Brightness > 0.6f) vote(MaterialClass::Lava, TextureStats * 0.5f, "red, bright");

    // polygon IDs: games give characters IDs of their own (edge marking
    // outlines each ID) while the scenery shares one. Drawn apart from the
    // scenery, a texture of many colours (a character's atlas) is a character
    if (e.SceneryId >= 0 && e.PolygonIds && !(e.PolygonIds >> e.SceneryId & 1) && s.Variety >= 0.05f)
        vote(MaterialClass::Character, RenderState, "own polygon ID, many colours");

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
