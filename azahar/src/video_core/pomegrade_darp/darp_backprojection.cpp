// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.7: back-projection (after Irani and Peleg, 1991). Reducing the reconstruction with a box
// filter must give back the reference, within its quantization interval: each block's error is
// removed from all of its texels (nearest-neighbour spread, which corrects a box average exactly).
// Then every output texel is kept within the colours of its reference neighbourhood, so nothing
// outside the texture's own palette of nearby colours appears.

#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

void BackProject(Image& upscaled, const Image& reference, const Colour& tolerance) {
    const int factor = upscaled.width / reference.width;
    if (factor < 2) {
        return;
    }
    const Image reduced = BoxDownsample(upscaled, factor);
    for (int y = 0; y < upscaled.height; y++) {
        for (int x = 0; x < upscaled.width; x++) {
            const Colour& low = reduced.At(x / factor, y / factor);
            const Colour& target = reference.At(x / factor, y / factor);
            Colour& out = upscaled.At(x, y);
            for (int c = 0; c < 4; c++) {
                const float allowed =
                    std::clamp(low[c], target[c] - tolerance[c], target[c] + tolerance[c]);
                out[c] -= low[c] - allowed;
            }
        }
    }
}

void ClampToEnvelope(Image& upscaled, const Image& reference) {
    const int factor = upscaled.width / reference.width;
    for (int y = 0; y < upscaled.height; y++) {
        for (int x = 0; x < upscaled.width; x++) {
            // the reference texel under this output texel and the one it leans towards
            const int rx = x / factor;
            const int ry = y / factor;
            const int nx = (x % factor) * 2 < factor ? rx - 1 : rx + 1;
            const int ny = (y % factor) * 2 < factor ? ry - 1 : ry + 1;
            Colour low = reference.Fetch(rx, ry);
            Colour high = low;
            for (const auto& [px, py] : {std::pair{nx, ry}, std::pair{rx, ny}, std::pair{nx, ny}}) {
                const Colour& texel = reference.Fetch(px, py);
                for (int c = 0; c < 4; c++) {
                    low[c] = std::min(low[c], texel[c]);
                    high[c] = std::max(high[c], texel[c]);
                }
            }
            Colour& out = upscaled.At(x, y);
            for (int c = 0; c < 4; c++) {
                out[c] = std::clamp(out[c], low[c], high[c]);
            }
        }
    }
}

} // namespace Pomegrade::Darp
