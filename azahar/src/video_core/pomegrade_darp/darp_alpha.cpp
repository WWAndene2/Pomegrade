// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.8: an alpha that is a shape (foliage, fences, hair) is not blurred: the signed distance
// to the shape's edge (after Green, Valve, 2007) varies smoothly, so it is the distance that gets
// enlarged, and the alpha is rebuilt from it with an edge one output texel wide. Before that, the
// colour of transparent texels is filled in from their opaque neighbours so edges get no dark
// fringe.

#include <cmath>
#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

namespace {

constexpr int DistanceRadius = 4;

bool Inside(const Colour& texel) {
    return texel[3] >= 0.5f;
}

/// Signed distance from each texel centre to the shape's edge, in texels: > 0 inside.
std::vector<float> SignedDistance(const Image& source) {
    std::vector<float> distance(source.texels.size());
    for (int y = 0; y < source.height; y++) {
        for (int x = 0; x < source.width; x++) {
            const bool inside = Inside(source.At(x, y));
            float nearest = static_cast<float>(DistanceRadius) + 0.5f;
            for (int dy = -DistanceRadius; dy <= DistanceRadius; dy++) {
                for (int dx = -DistanceRadius; dx <= DistanceRadius; dx++) {
                    if (Inside(source.Fetch(x + dx, y + dy)) != inside) {
                        nearest =
                            std::min(nearest, std::sqrt(static_cast<float>(dx * dx + dy * dy)));
                    }
                }
            }
            // the edge lies half a texel before the nearest texel of the other side
            const float edge = nearest - 0.5f;
            distance[static_cast<std::size_t>(y) * source.width + x] = inside ? edge : -edge;
        }
    }
    return distance;
}

} // Anonymous namespace

void FillTransparentColours(Image& image) {
    constexpr int Passes = 8;
    std::vector<bool> known(image.texels.size());
    for (std::size_t i = 0; i < image.texels.size(); i++) {
        known[i] = Inside(image.texels[i]);
    }
    for (int pass = 0; pass < Passes; pass++) {
        std::vector<bool> next_known = known;
        Image next = image;
        for (int y = 0; y < image.height; y++) {
            for (int x = 0; x < image.width; x++) {
                const std::size_t index = static_cast<std::size_t>(y) * image.width + x;
                if (known[index]) {
                    continue;
                }
                Colour sum{};
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        const int nx = Image::WrapCoordinate(x + dx, image.width, image.wrap_s);
                        const int ny = Image::WrapCoordinate(y + dy, image.height, image.wrap_t);
                        if (known[static_cast<std::size_t>(ny) * image.width + nx]) {
                            const Colour& texel = image.At(nx, ny);
                            for (int c = 0; c < 3; c++) {
                                sum[c] += texel[c];
                            }
                            count++;
                        }
                    }
                }
                if (count > 0) {
                    for (int c = 0; c < 3; c++) {
                        next.At(x, y)[c] = sum[c] / static_cast<float>(count);
                    }
                    next_known[index] = true;
                }
            }
        }
        image = std::move(next);
        known = std::move(next_known);
    }
}

void ApplyAlphaShape(Image& upscaled, const Image& source) {
    const std::vector<float> distance = SignedDistance(source);
    const float factor = static_cast<float>(upscaled.width) / static_cast<float>(source.width);
    const auto fetch = [&](int x, int y) {
        const int wx = Image::WrapCoordinate(x, source.width, source.wrap_s);
        const int wy = Image::WrapCoordinate(y, source.height, source.wrap_t);
        return distance[static_cast<std::size_t>(wy) * source.width + wx];
    };
    for (int y = 0; y < upscaled.height; y++) {
        for (int x = 0; x < upscaled.width; x++) {
            // bilinear sample of the distance at the output texel's centre
            const float u = (static_cast<float>(x) + 0.5f) / factor - 0.5f;
            const float v = (static_cast<float>(y) + 0.5f) / factor - 0.5f;
            const int x0 = static_cast<int>(std::floor(u));
            const int y0 = static_cast<int>(std::floor(v));
            const float fx = u - static_cast<float>(x0);
            const float fy = v - static_cast<float>(y0);
            const float top = fetch(x0, y0) * (1.f - fx) + fetch(x0 + 1, y0) * fx;
            const float bottom = fetch(x0, y0 + 1) * (1.f - fx) + fetch(x0 + 1, y0 + 1) * fx;
            const float d = top * (1.f - fy) + bottom * fy;
            // an edge one output texel wide
            upscaled.At(x, y)[3] = std::clamp(0.5f + d * factor, 0.f, 1.f);
        }
    }
}

} // namespace Pomegrade::Darp
