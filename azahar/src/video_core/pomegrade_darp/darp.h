// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Pomegrade DARP v2: deterministic, engine-guided reconstruction of a texture at x2 to x16,
// without learning. Pure CPU code with no emulator dependency, so it is reproducible and testable
// on its own. The design is the owner's "DARP v2" thesis (section numbers below refer to it); the
// steps run once per texture, off the emulation thread (see VideoCore::DarpManager).
//
//   Classify (s.3) -> dequantize (s.4) -> per x2 level: directional kernel (s.5), local
//   self-examples (s.6), back-projection and envelope clamp (s.7) -> alpha shapes from a signed
//   distance field (s.8) -> palette snap for pixel art (annex A). Edges follow the texture's wrap
//   modes (s.10).
//
// Deviations from the thesis, decided while implementing: interpolation and back-projection work
// in the texture's stored (gamma) values, where the quantization intervals are defined, rather
// than in linear light; colour distances use luma-weighted RGB plus alpha. Back-projection
// spreads the error with nearest-neighbour (exact for the box downsampler) instead of a
// directional upsampling.

#include <optional>
#include <vector>
#include "common/common_types.h"

namespace Pomegrade::Darp {

/// The texture's format as the 3DS stores it, which sets its quantization (thesis s.4).
enum class SourceFormat : u32 {
    RGBA8,
    RGB8,
    RGB5A1,
    RGB565,
    RGBA4,
    IA8,
    HILO8, ///< two-channel data (normal maps): never reconstructed
    I8,
    A8,
    IA4,
    I4,
    A4,
    ETC1,
    ETC1A4,
};

/// How the texture repeats beyond its edges (the sampler's wrap mode; border counts as clamp).
enum class Wrap : u32 {
    Clamp,
    Repeat,
    Mirror,
};

/// What a texture is, which decides how it is reconstructed (thesis s.3).
enum class TextureClass : u32 {
    Data,       ///< technical data: left to the regular path
    Small,      ///< 8 texels or less on a side: strict pixel art
    PixelArt,   ///< few colours, flat areas
    Continuous, ///< gradients, many colours
};

struct Classification {
    TextureClass colour = TextureClass::Continuous;
    bool alpha_mask = false; ///< alpha is a shape (nearly all 0 or 255), see thesis s.8
};

/// An RGBA8 texture, rows top to bottom.
struct Texture {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba;
};

struct Options {
    u32 factor = 2; ///< 2, 4, 8 or 16
    SourceFormat format = SourceFormat::RGBA8;
    Wrap wrap_s = Wrap::Clamp;
    Wrap wrap_t = Wrap::Clamp;
};

/// Classifies a texture from its content and format (thesis s.3).
Classification Classify(const Texture& texture, SourceFormat format);

/// Largest side of a DARP result: the factor is halved until the result fits (see darp_upscale).
constexpr u32 MaxOutputSide = 2048;

/**
 * Reconstructs the texture at options.factor times its size (less if it would exceed
 * MaxOutputSide). Returns nothing for data textures
 * (TextureClass::Data) or an invalid input, which the caller leaves to the regular path.
 * Deterministic: the same input always gives the same output.
 */
std::optional<Texture> Upscale(const Texture& source, const Options& options);

/// Bumped whenever the output of Upscale changes, so cached results are not reused.
constexpr u32 Version = 1;

} // namespace Pomegrade::Darp
