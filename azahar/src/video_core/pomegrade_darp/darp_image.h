// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// DARP internals: a floating-point RGBA image addressed with the texture's wrap modes, and the
// small helpers every step shares.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "video_core/pomegrade_darp/darp.h"

namespace Pomegrade::Darp {

using Colour = std::array<float, 4>; ///< RGBA, 0..1, stored (gamma) values

struct Image {
    int width = 0;
    int height = 0;
    Wrap wrap_s = Wrap::Clamp;
    Wrap wrap_t = Wrap::Clamp;
    std::vector<Colour> texels;

    Image() = default;
    Image(int width_, int height_, Wrap wrap_s_, Wrap wrap_t_)
        : width{width_}, height{height_}, wrap_s{wrap_s_}, wrap_t{wrap_t_},
          texels(static_cast<std::size_t>(width_) * height_) {}

    /// Same size and wrap modes, texels zeroed.
    Image SameShape() const {
        return Image(width, height, wrap_s, wrap_t);
    }

    Colour& At(int x, int y) {
        return texels[static_cast<std::size_t>(y) * width + x];
    }
    const Colour& At(int x, int y) const {
        return texels[static_cast<std::size_t>(y) * width + x];
    }

    /// The texel at (x, y), out-of-range coordinates resolved with the wrap modes.
    const Colour& Fetch(int x, int y) const {
        return At(WrapCoordinate(x, width, wrap_s), WrapCoordinate(y, height, wrap_t));
    }

    static int WrapCoordinate(int value, int size, Wrap wrap) {
        switch (wrap) {
        case Wrap::Repeat:
            value %= size;
            return value < 0 ? value + size : value;
        case Wrap::Mirror: {
            const int period = size * 2;
            value %= period;
            if (value < 0) {
                value += period;
            }
            return value < size ? value : period - 1 - value;
        }
        case Wrap::Clamp:
        default:
            return std::clamp(value, 0, size - 1);
        }
    }
};

/// Perceptual distance between two colours: luma-weighted RGB, plus alpha.
inline float Distance(const Colour& a, const Colour& b) {
    const float dr = a[0] - b[0];
    const float dg = a[1] - b[1];
    const float db = a[2] - b[2];
    const float da = a[3] - b[3];
    return std::sqrt(0.299f * dr * dr + 0.587f * dg * dg + 0.114f * db * db + 0.5f * da * da);
}

inline Colour Mix(const Colour& a, float wa, const Colour& b, float wb) {
    return {a[0] * wa + b[0] * wb, a[1] * wa + b[1] * wb, a[2] * wa + b[2] * wb,
            a[3] * wa + b[3] * wb};
}

Image FromTexture(const Texture& texture, Wrap wrap_s, Wrap wrap_t);
Texture ToTexture(const Image& image);

/// Box average of factor x factor blocks (the downsampler back-projection inverts).
Image BoxDownsample(const Image& image, int factor);

/// Per-channel half-width of the format's quantization interval, in 0..1 (thesis s.4).
Colour QuantizationHalfWidth(SourceFormat format);

// The steps, in pipeline order
Image Dequantize(const Image& source, SourceFormat format);                         // s.4
Image DirectionalKernel(const Image& image, float flat_threshold);                  // s.5
void AddSelfExamples(Image& upscaled, const Image& image, float flat_threshold);    // s.6
void BackProject(Image& upscaled, const Image& reference, const Colour& tolerance); // s.7
void ClampToEnvelope(Image& upscaled, const Image& reference);                      // s.7
void FillTransparentColours(Image& image);                                          // s.8
void ApplyAlphaShape(Image& upscaled, const Image& source);                         // s.8
void SnapToLocalPalette(Image& upscaled, const Image& source);                      // annex A

} // namespace Pomegrade::Darp
