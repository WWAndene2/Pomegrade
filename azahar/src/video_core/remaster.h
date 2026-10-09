// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

// Pomegrade: Remaster, a post-process of the top screen, once per emulated frame (Settings::values.remaster_preset), after
// the offline ORAS renders of tools/remake/render (ORAS_RENDER.md: the v7 preset the owner validated, v9 after it). Those
// renders path-trace light, ray-trace occlusion and know each texture's material; a frame of the emulator has only its
// colours and its depth buffer, so each technique is done here from those two, live:
// - two-tone light: the v7 step (smoothstep 0.60 -> 0.78) on the frame's luminance, a cool lavender shade and a warm sun
//   (offline: on the path-traced light);
// - ambient occlusion: screen-space, from the depth buffer, applied as v9 does, tone *= 1 - (1 - ao) (0.55 - 0.4 step),
//   strong in the shade and weak in the sun (offline: ray-traced in world space);
// - sky fill (v9): (0.010, 0.018, 0.045) added in the open shade; sky light (v7 layers): a cool light on surfaces facing
//   up, their normal rebuilt from the depth;
// - contours on depth breaks, glow on highlights, aerial perspective, slight far blur (v7 layers);
// - vibrance 1.28 (v7); contrast: an S-curve, 0.7 in v7, adaptive in v9: clip(0.7 x 0.2 / std(luminance), 0.4, 1.0) (the
//   sun's elevation term is left out: a frame does not tell the sun's height).
// Not done live: the texture relief, parallax occlusion and volumetric layered depth (they need each texture's material,
// recognised offline), the path-traced light and the interiors seen through windows. Without a depth buffer for the frame
// (a frame copied by the CPU), only the colour effects apply. OpenGL only.
namespace VideoCore::Remaster {

struct Params {
    bool enabled = false;
    float grading = 0;   ///< two-tone light, 0-1
    float ao = 0;        ///< occlusion strength, 0-1
    float sky_fill = 0;  ///< v9's sky fill in the open shade, 0-1
    float sky_light = 0; ///< light on up-facing surfaces, 0-1
    float outline = 0;   ///< contours, 0-1
    float glow = 0;      ///< glow, 0-1
    float aerial = 0;    ///< aerial perspective, 0-1
    float far_blur = 0;  ///< far blur, 0-1
    float vibrance = 1;  ///< 1 none, 1.28 v7
    float contrast = 0;  ///< the S-curve's strength; ignored when adaptive
    bool adaptive_contrast = false;
};

/// The current preset's parameters (off, v7, v9, or custom from the switches)
Params Current();

/// The shaders (no #version: OpenGL's LoadShader adds it). STATS: the frame's luminance and its square (r, g), for the
/// mean and spread read in its smallest mipmap; BRIGHT: the highlights, blurred by their mipmaps for the glow; MAIN: the
/// remastered frame. Bindings: 0 the frame, 1 its depth, 2 the stats, 3 the highlights.
extern const char* const STATS_FRAG;
extern const char* const BRIGHT_FRAG;
extern const char* const MAIN_FRAG;
extern const char* const FULLSCREEN_VERT;

/// A shader's source with the preamble the fragment shaders share
std::string Source(const char* body);

} // namespace VideoCore::Remaster
