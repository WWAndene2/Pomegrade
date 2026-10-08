// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.9: the cascade. x4, x8 and x16 are successive x2 levels; each level is back-projected
// on the level before it and on the source (so errors cannot pile up) and kept within its
// envelope. Self-examples only run on the first two levels: past them the texture has no detail
// left to lend, and later levels get a single back-projection pass (they only keep edges).
// The output is capped at MaxSide texels a side (a float image of 2048x2048 is 64 MB, and the
// pipeline holds a few): the factor is halved until it fits, as the GPU upscaling does with its
// own cap.

#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

namespace {

constexpr int BackProjectionPasses = 3;
constexpr u32 MaxSide = MaxOutputSide;
constexpr int SelfExampleLevels = 2;

bool ValidFactor(u32 factor) {
    return factor == 2 || factor == 4 || factor == 8 || factor == 16;
}

} // Anonymous namespace

std::optional<Texture> Upscale(const Texture& source_texture, const Options& options) {
    if (!ValidFactor(options.factor) || source_texture.width == 0 || source_texture.height == 0 ||
        source_texture.rgba.size() !=
            static_cast<std::size_t>(source_texture.width) * source_texture.height * 4) {
        return std::nullopt;
    }
    const Classification classification = Classify(source_texture, options.format);
    if (classification.colour == TextureClass::Data) {
        return std::nullopt;
    }
    const bool pixel_art = classification.colour == TextureClass::PixelArt ||
                           classification.colour == TextureClass::Small;

    const Image original = FromTexture(source_texture, options.wrap_s, options.wrap_t);
    Image stored = original;
    if (classification.alpha_mask) {
        FillTransparentColours(stored);
    }

    u32 factor_limit = options.factor;
    while (factor_limit > 2 &&
           std::max(source_texture.width, source_texture.height) * factor_limit > MaxSide) {
        factor_limit /= 2;
    }

    const Colour tolerance = QuantizationHalfWidth(options.format);
    // flat when no gradient exceeds one quantization step of the colour channels (s.5.4)
    const float flat_threshold = std::max({tolerance[0], tolerance[1], tolerance[2]}) * 2.f;
    const Colour level_tolerance = QuantizationHalfWidth(SourceFormat::RGBA8);

    Image current = pixel_art ? stored : Dequantize(stored, options.format);
    int level = 0;
    for (u32 factor = 2; factor <= factor_limit; factor *= 2, level++) {
        Image upscaled = DirectionalKernel(current, flat_threshold);
        if (!pixel_art && level < SelfExampleLevels) {
            AddSelfExamples(upscaled, current, flat_threshold);
        }
        const int passes = level < SelfExampleLevels ? BackProjectionPasses : 1;
        for (int pass = 0; pass < passes; pass++) {
            BackProject(upscaled, current, level_tolerance);
            BackProject(upscaled, stored, tolerance);
            ClampToEnvelope(upscaled, current);
        }
        current = std::move(upscaled);
    }

    if (pixel_art) {
        SnapToLocalPalette(current, stored);
    }
    if (classification.alpha_mask) {
        ApplyAlphaShape(current, original);
    }
    return ToTexture(current);
}

} // namespace Pomegrade::Darp
