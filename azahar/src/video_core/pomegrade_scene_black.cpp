// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "video_core/pomegrade_scene_black.h"

namespace VideoCore {

Layout::FramebufferLayout SceneBlack::MeasurementLayout() {
    Layout::FramebufferLayout layout{};
    layout.width = Width;
    layout.height = Height;
    layout.top_screen_enabled = true;
    layout.bottom_screen_enabled = true;
    layout.top_screen = {0, 0, Width / 2, Height};
    layout.bottom_screen = {Width / 2, 0, Width, Height};
    layout.is_rotated = true;
    layout.render_3d_mode = Settings::StereoRenderOption::Off;
    return layout;
}

void SceneBlack::Measure(const u8* pixels, bool bgra) {
    const u32 r_index = bgra ? 2 : 0;
    const u32 b_index = bgra ? 0 : 2;
    for (u32 half = 0; half < 2; half++) {
        std::array<u32, 256> histogram{};
        for (u32 y = 0; y < Height; y++) {
            for (u32 x = half * Width / 2; x < (half + 1) * Width / 2; x++) {
                const u8* pixel = pixels + (y * Width + x) * 4;
                const float luma =
                    0.2126f * pixel[r_index] + 0.7152f * pixel[1] + 0.0722f * pixel[b_index];
                histogram[std::min(255, static_cast<int>(std::lround(luma)))]++;
            }
        }
        // the 2nd percentile of luma: the scene's own black, ignoring a few stray pixels
        const u32 count = Width / 2 * Height;
        const u32 wanted = count * 2 / 100;
        u32 sum = 0;
        u32 black = 255;
        for (u32 i = 0; i < 256; i++) {
            sum += histogram[i];
            if (sum > wanted) {
                black = i;
                break;
            }
        }
        target[half] = TargetThreshold(black / 255.f);
    }
}

float SceneBlack::TargetThreshold(float black_level) {
    // the scene's darkest shades go to true black: a dark grey background turns off on an OLED
    // screen, a scene that is already black only loses its very darkest shades
    return std::clamp(black_level * 1.5f + 0.03f, 0.03f, 0.12f);
}

void SceneBlack::Step() {
    // ~8% of the way per frame: settles in about half a second at 60 fps
    constexpr float rate = 0.08f;
    for (u32 i = 0; i < 2; i++) {
        current[i] += (target[i] - current[i]) * rate;
    }
}

void SceneBlack::Reset() {
    target = {};
    current = {};
}

} // namespace VideoCore
