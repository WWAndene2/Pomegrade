// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Pomegrade texture upscaling: the games' textures enlarged by their own factor (Texture upscaling
// setting), independent of the internal resolution that upstream ties the texture filters to.
// Render targets keep the internal resolution: only surfaces created as textures use this scale.

#include <algorithm>
#include "common/common_types.h"
#include "common/settings.h"

namespace VideoCore {

/// Largest side of an upscaled texture: 4096 texels (64 MB in RGBA8), so that x16 on a large
/// texture doesn't run a phone out of memory. Every GPU Azahar runs on supports 4096.
constexpr u32 MaxUpscaledTextureSide = 4096;

/// The algorithm the GPU enlarges textures with: the Texture Filter setting, xBRZ when that is
/// NoFilter and Texture upscaling is on (so the setting always has a visible effect), and xBRZ for
/// DARP, which runs on the CPU and replaces the GPU's result once ready (pomegrade_darp_manager.h).
inline Settings::TextureFilter TextureUpscalingFilter() {
    const auto filter = Settings::values.texture_filter.GetValue();
    if (filter == Settings::TextureFilter::DARP) {
        return Settings::TextureFilter::xBRZ;
    }
    if (filter == Settings::TextureFilter::NoFilter &&
        Settings::values.texture_upscale_factor.GetValue() > 1) {
        return Settings::TextureFilter::xBRZ;
    }
    return filter;
}

/// The resolution scale of a new texture surface of width x height texels. With Texture upscaling
/// on: its factor, halved until the texture fits MaxUpscaledTextureSide (at least 1). Off: the
/// upstream behaviour, the internal resolution when a texture filter is chosen, else 1.
inline u32 TextureUpscalingScale(u32 width, u32 height, u32 resolution_scale_factor) {
    u32 factor = Settings::values.texture_upscale_factor.GetValue();
    if (factor <= 1) {
        return Settings::values.texture_filter.GetValue() != Settings::TextureFilter::NoFilter
                   ? resolution_scale_factor
                   : 1;
    }
    const u32 side = std::max(width, height);
    while (factor > 1 && side * factor > MaxUpscaledTextureSide) {
        factor /= 2;
    }
    return factor;
}

} // namespace VideoCore
