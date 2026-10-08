// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis annex A: pixel art keeps its palette. Each output texel takes the colour, among the 3x3
// source texels around it, closest to its reconstructed value: shapes follow the directional
// kernel, colours stay the artist's.

#include <limits>
#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

void SnapToLocalPalette(Image& upscaled, const Image& source) {
    const int factor = upscaled.width / source.width;
    for (int y = 0; y < upscaled.height; y++) {
        for (int x = 0; x < upscaled.width; x++) {
            Colour& out = upscaled.At(x, y);
            const int sx = x / factor;
            const int sy = y / factor;
            float best = std::numeric_limits<float>::max();
            Colour chosen = source.At(sx, sy);
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    const Colour& candidate = source.Fetch(sx + dx, sy + dy);
                    const float distance = Distance(out, candidate);
                    if (distance < best) {
                        best = distance;
                        chosen = candidate;
                    }
                }
            }
            out = chosen;
        }
    }
}

} // namespace Pomegrade::Darp
