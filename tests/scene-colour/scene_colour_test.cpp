#undef NDEBUG // the checks below are asserts
#include "GPU_SceneColour.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace melonDS;

static u32 Grey(int v) { return v | (v << 8) | (v << 16) | (255u << 24); }

int main()
{
    // a washed-out scene: greys from 80 to 180 only
    std::vector<u32> dull;
    for (int i = 0; i < 1000; i++) dull.push_back(Grey(80 + i % 101));
    SceneStats s = SceneColour::Measure(dull.data(), dull.size());
    printf("dull: black %.3f white %.3f saturation %.3f\n", s.Black, s.White, s.Saturation);
    assert(s.Black > 0.3f && s.White < 0.72f && s.Saturation == 0);
    SceneColourParams p = SceneColour::Target(s, true, false);
    printf("  -> levels %.3f..%.3f, saturation x%.2f\n", p.LevelBlack, p.LevelWhite, p.Saturation);
    assert(p.LevelBlack > 0.1f && p.LevelWhite < 0.9f && p.Saturation > 1.2f);
    // contrast goes up: the scene's black and white move apart
    float lo[3] = {80/255.f, 80/255.f, 80/255.f}, hi[3] = {180/255.f, 180/255.f, 180/255.f};
    SceneColour::Apply(p, lo); SceneColour::Apply(p, hi);
    printf("  -> greys 80 and 180 become %.0f and %.0f\n", lo[0]*255, hi[0]*255);
    assert(hi[0] - lo[0] > (180 - 80) / 255.f * 1.3f);

    // a full-range, colourful scene is left almost alone
    std::vector<u32> full;
    for (int i = 0; i < 512; i++) full.push_back(Grey(i % 256)); // black to white
    for (int i = 0; i < 512; i++) full.push_back(i % 2 ? (255u | (255u << 24)) : ((255u << 16) | (255u << 24))); // pure red, pure blue
    s = SceneColour::Measure(full.data(), full.size());
    p = SceneColour::Target(s, true, false);
    printf("full range: levels %.3f..%.3f, saturation x%.2f\n", p.LevelBlack, p.LevelWhite, p.Saturation);
    assert(p.LevelBlack < 0.05f && p.LevelWhite > 0.95f && p.Saturation == 1);

    // OLED: a dark grey background (luma ~0.06) goes to black, mid tones stay
    std::vector<u32> dark(1000, Grey(16));
    for (int i = 0; i < 300; i++) dark[i] = Grey(60 + i % 190);
    s = SceneColour::Measure(dark.data(), dark.size());
    p = SceneColour::Target(s, false, true);
    printf("dark scene: black %.3f -> OLED threshold %.3f\n", s.Black, p.OledThreshold);
    float bg[3] = {16/255.f, 16/255.f, 16/255.f}, mid[3] = {0.5f, 0.5f, 0.5f};
    SceneColour::Apply(p, bg); SceneColour::Apply(p, mid);
    printf("  -> background %.4f, mid grey %.3f\n", bg[0], mid[0]);
    assert(bg[0] < 1/255.f && std::fabs(mid[0] - 0.5f) < 1e-6f);
    // an already black scene only loses its very darkest shades
    std::vector<u32> black(1000, Grey(0));
    for (int i = 0; i < 300; i++) black[i] = Grey(40 + i % 200);
    p = SceneColour::Target(SceneColour::Measure(black.data(), black.size()), false, true);
    printf("black scene: OLED threshold %.3f\n", p.OledThreshold);
    assert(p.OledThreshold <= 0.031f);

    // neutral parameters change nothing; black and white stay put
    SceneColourParams neutral;
    for (float v : {0.f, 0.2f, 0.7f, 1.f})
    {
        float c[3] = {v, v * 0.5f, 1 - v};
        float o[3] = {c[0], c[1], c[2]};
        SceneColour::Apply(neutral, o);
        for (int k = 0; k < 3; k++) assert(std::fabs(o[k] - c[k]) < 1e-6f);
    }

    // parameters move smoothly: about 8% per frame
    SceneColourParams cur, target; target.OledThreshold = 0.1f;
    SceneColour::Step(cur, target);
    assert(std::fabs(cur.OledThreshold - 0.008f) < 1e-6f);
    for (int i = 0; i < 60; i++) SceneColour::Step(cur, target);
    assert(std::fabs(cur.OledThreshold - 0.1f) < 0.001f);

    puts("ALL OK");
}
