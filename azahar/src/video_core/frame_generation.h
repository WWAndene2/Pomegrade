// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

// Pomegrade: the frame rate modes of the 3DS (Settings::values.frame_rate_mode), as the DS has them. The game always runs
// at its own speed; what changes is the images the screen gets:
// - 30: one distinct image in two shown (saves battery);
// - 60: the game as it is;
// - 60 smooth, 120, 240: images generated between the last two distinct images, at most 60, 120 or 240 shown per second
//   and never more than the screen shows. A 30 fps game (each image drawn twice: Core::PerfStats::game_frames_updated
//   is false for the second) gets one image between two at 60. Generated images are interpolated between two real ones,
//   so what is shown runs one distinct image late (about 17 ms at 60 fps, 33 at 30);
// - adaptive: as 240 capped by the screen, and as 60 while the phone is hot (the frontend reports it).
namespace VideoCore::FrameGeneration {

enum class Mode : u32 { Fps30 = 0, Fps60 = 1, Fps60Smooth = 2, Fps120 = 3, Fps240 = 4, Adaptive = 5 };

Mode CurrentMode();

/// The frontend's report: the screen's refresh rate (0 when unknown, taken as 60) and whether the phone is hot
void SetDisplay(float refresh_hz, bool hot);

/// Whether only one distinct image in two is shown (30 fps mode)
bool ShowsHalf();

/// How many images to generate between two distinct images `interval_s` seconds apart (0: none), from the mode's cap and
/// the screen's rate: the slots the screen has in that interval, less the real image's
u32 ImagesBetween(double interval_s);

/// Whether images are generated at all in the current mode and state
bool Generates();

/// The interpolation's fragment shaders (no #version: each renderer adds its own preamble, and POMEGRADE_VULKAN 1 or 0):
/// - DOWNSAMPLE: the luma of images A (r) and B (g) at 1/8 size (4 bilinear taps each);
/// - MOTION: per small texel, the motion from A to B (in small texels, xy) and its match cost (z), found by a
///   symmetric search: A at p - t v against B at p + (1 - t) v, for the position t of the image to generate;
/// - WARP: per screen pixel, A and B moved along the motion and blended by t; where they disagree (an occlusion, a
///   change the motion can't explain) the nearer real image is taken instead of a ghost of both.
/// Parameters: P0 = (t, 1 / small width, 1 / small height, 0), P1 = (1 / width, 1 / height, 0, 0).
/// Bindings: 0 image A, 1 image B, 2 the small luma texture (MOTION) or the motion texture (WARP).
extern const char* const DOWNSAMPLE_FRAG;
extern const char* const MOTION_FRAG;
extern const char* const WARP_FRAG;
/// A full-screen triangle (gl_VertexIndex / gl_VertexID 0-2), frag_tex_coord in [0, 1]
extern const char* const FULLSCREEN_VERT;

/// The shader source for a renderer: `vulkan` puts "#version 450 core" first (OpenGL's LoadShader adds its own)
std::string Source(const char* body, bool vulkan);

} // namespace VideoCore::FrameGeneration
