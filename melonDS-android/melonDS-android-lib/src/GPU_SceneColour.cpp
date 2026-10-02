#include "GPU_SceneColour.h"

#include <algorithm>
#include <cmath>

namespace melonDS
{

namespace
{

float Luma(float r, float g, float b)
{
    return 0.2126f*r + 0.7152f*g + 0.0722f*b;
}

float Clamp01(float v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

float Smoothstep(float from, float to, float v)
{
    float x = Clamp01((v - from) / (to - from));
    return x*x*(3 - 2*x);
}

}

SceneStats SceneColour::Measure(const u32* rgba, u32 count)
{
    SceneStats stats;
    if (count == 0)
        return stats;

    u32 histogram[256] = {};
    double saturation = 0;
    for (u32 i = 0; i < count; i++)
    {
        float r = (rgba[i] & 0xFF) / 255.f, g = ((rgba[i] >> 8) & 0xFF) / 255.f, b = ((rgba[i] >> 16) & 0xFF) / 255.f;
        histogram[std::min(255, (int)std::lround(Luma(r, g, b) * 255))]++;
        saturation += std::max({r, g, b}) - std::min({r, g, b});
    }

    auto percentile = [&](double p) {
        u32 target = (u32)(p * count), sum = 0;
        for (int i = 0; i < 256; i++)
        {
            sum += histogram[i];
            if (sum > target) return i / 255.f;
        }
        return 1.f;
    };
    stats.Black = percentile(0.02);
    stats.White = percentile(0.98);
    stats.Saturation = (float)(saturation / count);
    return stats;
}

SceneColourParams SceneColour::Target(const SceneStats& stats, bool adaptive, bool oled)
{
    SceneColourParams target;

    if (adaptive)
    {
        // stretch part of the way: a washed-out scene (black level raised, white
        // level lowered) gains contrast; a full-range one is left as is
        target.LevelBlack = std::min(stats.Black, 0.25f) * 0.6f;
        target.LevelWhite = 1 - (1 - std::max(stats.White, 0.5f)) * 0.6f;
        // dull scenes get a bit more colour, up to +30%
        target.Saturation = stats.Saturation < 0.25f ? 1 + std::min(0.3f, (0.25f - stats.Saturation) * 1.5f) : 1;
    }

    if (oled)
    {
        // the scene's own black (its darkest 2%) goes to true black: a dark grey
        // background turns off on an OLED screen, a scene that is already black
        // only loses its very darkest shades
        target.OledThreshold = std::clamp(stats.Black * 1.5f + 0.03f, 0.03f, 0.12f);
    }

    return target;
}

void SceneColour::Step(SceneColourParams& current, const SceneColourParams& target)
{
    // ~8% of the way per frame: settles in about half a second at 60 fps
    const float rate = 0.08f;
    current.LevelBlack += (target.LevelBlack - current.LevelBlack) * rate;
    current.LevelWhite += (target.LevelWhite - current.LevelWhite) * rate;
    current.Saturation += (target.Saturation - current.Saturation) * rate;
    current.OledThreshold += (target.OledThreshold - current.OledThreshold) * rate;
}

void SceneColour::Apply(const SceneColourParams& params, float* rgb)
{
    // keep in sync with kCompositorFS_Nearest
    float range = std::max(params.LevelWhite - params.LevelBlack, 0.05f);
    for (int i = 0; i < 3; i++)
        rgb[i] = Clamp01((rgb[i] - params.LevelBlack) / range);

    float l = Luma(rgb[0], rgb[1], rgb[2]);
    for (int i = 0; i < 3; i++)
        rgb[i] = Clamp01(l + (rgb[i] - l) * params.Saturation);

    if (params.OledThreshold > 0)
    {
        float fade = Smoothstep(params.OledThreshold * 0.5f, params.OledThreshold, Luma(rgb[0], rgb[1], rgb[2]));
        for (int i = 0; i < 3; i++)
            rgb[i] *= fade;
    }
}

}
