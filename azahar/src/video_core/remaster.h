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
// - the path-traced light's live part, a light pass at half size from the depth buffer: occlusion, contact shadows
//   toward the sun and the coloured bounce of the surroundings, denoised by a depth-guided bilateral filter (offline:
//   path-traced and ray-traced in world space, median and guided filter); applied as v9 does, tone *= 1 - (1 - ao)
//   (0.55 - 0.4 step), the bounce as compose.py, 1 + 0.3 (tint - 1)(1 - 0.5 step). The sun is taken up the screen: the
//   post-process has no light direction (the game's own clock-driven light and shadows stay underneath);
// - sky fill (v9): (0.010, 0.018, 0.045) added in the open shade; sky light (v7 layers): a cool light on surfaces facing
//   up, their normal rebuilt from the depth;
// - warm rim (layers.py): silhouettes turned to the sun, (0.28, 0.2, 0.1) x (0.3 + luminance);
// - contours on depth breaks, glow on highlights, aerial perspective, slight far blur (v7 layers);
// - vibrance 1.28 (v7); contrast: an S-curve, 0.7 in v7, adaptive in v9: clip(0.7 x 0.2 / std(luminance), 0.4, 1.0) (the
//   sun's elevation term is left out: a frame does not tell the sun's height).
// Each texture's material is recognised once, when the game uploads it (SurfaceMode): relief, parallax, volume, its
// painted shadows lifted and a lit room behind window glass, in the textures' own shader. Not done: a world-space bake
// of the light per map piece (the screen-space pass stands in for it). Without a depth buffer for the frame
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
    float bounce = 0;         ///< coloured bounce, 0-1
    float contact_shadow = 0; ///< contact shadows toward the sun, 0-1
    float rim = 0;            ///< warm rim on silhouettes, 0-1
};

/// The current preset's parameters (off, v7, v9, or custom from the switches)
Params Current();

/// The textures' surface shading of the current preset (glsl_fs_shader_gen's WritePomegradeSurface, from each texture's
/// MaterialRecognition maps): -1 none, 0 v7 (parallax, relief 2.2), 2 v9 (parallax, layered volume, no relief); custom
/// follows v9. Mode 1 (v8: volume and relief 1.1) has no preset.
int SurfaceMode();

/// The shaders (no #version: OpenGL's LoadShader adds it). STATS: the frame's luminance and its square (r, g), for the
/// mean and spread read in its smallest mipmap; BRIGHT: the highlights, blurred by their mipmaps for the glow; MAIN: the
/// remastered frame. LIGHT: occlusion, contact shadows and bounce at half size. Bindings: 0 the frame,
/// 1 its depth, 2 the stats, 7 the highlights, 8 the light.
extern const char* const STATS_FRAG;
extern const char* const BRIGHT_FRAG;
extern const char* const LIGHT_FRAG;
extern const char* const MAIN_FRAG;
extern const char* const FULLSCREEN_VERT;

/// A shader's source with the preamble the fragment shaders share
std::string Source(const char* body);

} // namespace VideoCore::Remaster
