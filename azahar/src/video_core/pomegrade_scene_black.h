// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Pomegrade scene-adaptive deep black (Deep black setting), shared by the OpenGL and Vulkan
// renderers. Each frame the renderer draws both screens, unadjusted, into a small measurement
// frame (top screen in its left half, bottom screen in its right half) and reads it back a frame
// later. From each screen's darkest shades comes a threshold under which the present shader fades
// pixels to true black (pixels off on OLED screens, deeper blacks on any screen). The thresholds
// move smoothly from frame to frame so the image doesn't pump. Same formula as the DS screen's
// OLED deep blacks (melonDS-android-lib GPU_SceneColour.cpp).

#include <array>
#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"

namespace VideoCore {

class SceneBlack {
public:
    static constexpr u32 Width = 64;  ///< Measurement frame width: two 32x32 halves
    static constexpr u32 Height = 32; ///< Measurement frame height

    /// The layout that draws the top screen into the left half, the bottom one into the right.
    static Layout::FramebufferLayout MeasurementLayout();

    /// Measures a read back measurement frame: Width x Height pixels of 4 bytes, rows in any
    /// order, RGBA (or BGRA when bgra is true).
    void Measure(const u8* pixels, bool bgra);

    /// Moves the thresholds a step towards the measured ones (once per presented frame).
    void Step();

    /// Forgets the scene (Deep black off): thresholds back to 0.
    void Reset();

    /// The luma under which pixels fade to black, 0 = off.
    float Threshold(bool bottom_screen) const {
        return current[bottom_screen];
    }

    /// The luma threshold for a scene whose darkest 2% are under black_level (0..1).
    static float TargetThreshold(float black_level);

private:
    std::array<float, 2> target{};
    std::array<float, 2> current{};
};

} // namespace VideoCore
