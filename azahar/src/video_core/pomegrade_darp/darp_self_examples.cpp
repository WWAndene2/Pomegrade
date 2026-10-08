// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.6: local self-examples (after Freedman and Fattal, 2011). The image I is reduced to D
// and D enlarged back with the same kernel to UD: the difference I - UD is the detail the kernel
// cannot invent, at I's own scale. For each texel q of the enlarged image U, the 3x3 patch of U
// around q is looked up in UD near q / 2 (a 5x5 search); the detail of the best match, taken from
// I - UD, is added to q. Every example comes from the texture itself: nothing is learned.

#include <limits>
#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

namespace {

constexpr int SearchRadius = 2;

using Patch = std::array<Colour, 9>;

Patch GetPatch(const Image& image, int x, int y) {
    Patch patch;
    int i = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            patch[i++] = image.Fetch(x + dx, y + dy);
        }
    }
    return patch;
}

float PatchDistance(const Patch& a, const Patch& b) {
    float sum = 0.f;
    for (int i = 0; i < 9; i++) {
        const float dr = a[i][0] - b[i][0], dg = a[i][1] - b[i][1];
        const float db = a[i][2] - b[i][2], da = a[i][3] - b[i][3];
        sum += 0.299f * dr * dr + 0.587f * dg * dg + 0.114f * db * db + 0.5f * da * da;
    }
    return sum;
}

/// Largest distance from the patch's centre: under the flat threshold, there is no detail to add.
float PatchSpread(const Patch& patch) {
    float spread = 0.f;
    for (int i = 0; i < 9; i++) {
        spread = std::max(spread, Distance(patch[4], patch[i]));
    }
    return spread;
}

} // Anonymous namespace

void AddSelfExamples(Image& upscaled, const Image& image, float flat_threshold) {
    if (image.width < 4 || image.height < 4) {
        return;
    }
    const Image reduced = BoxDownsample(image, 2);
    Image reduced_up = DirectionalKernel(reduced, flat_threshold);
    if (reduced_up.width != image.width || reduced_up.height != image.height) {
        return; // odd size: no exact 2x relation
    }

    Image detail = image.SameShape();
    for (std::size_t i = 0; i < detail.texels.size(); i++) {
        for (int c = 0; c < 4; c++) {
            detail.texels[i][c] = image.texels[i][c] - reduced_up.texels[i][c];
        }
    }

    // patches of the reduced-then-enlarged image, gathered once (each is searched many times)
    std::vector<Patch> candidates(reduced_up.texels.size());
    for (int y = 0; y < reduced_up.height; y++) {
        for (int x = 0; x < reduced_up.width; x++) {
            candidates[static_cast<std::size_t>(y) * reduced_up.width + x] =
                GetPatch(reduced_up, x, y);
        }
    }

    const Image base = upscaled; // matches are made on the kernel's output, not on added detail
    for (int y = 0; y < upscaled.height; y++) {
        for (int x = 0; x < upscaled.width; x++) {
            const Patch patch = GetPatch(base, x, y);
            if (PatchSpread(patch) < flat_threshold) {
                continue; // flat: its best match carries no detail
            }
            const int cx = x / 2;
            const int cy = y / 2;
            float best = std::numeric_limits<float>::max();
            int best_x = cx;
            int best_y = cy;
            for (int sy = -SearchRadius; sy <= SearchRadius; sy++) {
                for (int sx = -SearchRadius; sx <= SearchRadius; sx++) {
                    const int px =
                        Image::WrapCoordinate(cx + sx, reduced_up.width, reduced_up.wrap_s);
                    const int py =
                        Image::WrapCoordinate(cy + sy, reduced_up.height, reduced_up.wrap_t);
                    const float distance = PatchDistance(
                        patch, candidates[static_cast<std::size_t>(py) * reduced_up.width + px]);
                    if (distance < best) {
                        best = distance;
                        best_x = px;
                        best_y = py;
                    }
                }
            }
            const Colour& add = detail.At(best_x, best_y);
            Colour& out = upscaled.At(x, y);
            for (int c = 0; c < 4; c++) {
                out[c] += add[c];
            }
        }
    }
}

} // namespace Pomegrade::Darp
