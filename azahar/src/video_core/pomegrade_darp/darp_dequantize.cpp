// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.4: a stored value is an interval, not a value. Each texel is moved, inside its own
// interval, towards the average of its neighbours, so 16-bit staircases become ramps. Across an
// edge the intervals are far apart and the clamp keeps the texel where it was. This minimizes the
// squared variation (a smooth ramp) rather than the total variation the thesis names: same intent,
// simpler and stable.

#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

Image Dequantize(const Image& source, SourceFormat format) {
    constexpr int Iterations = 24;
    const Colour half_width = QuantizationHalfWidth(format);

    Image current = source;
    Image next = source.SameShape();
    for (int iteration = 0; iteration < Iterations; iteration++) {
        for (int y = 0; y < source.height; y++) {
            for (int x = 0; x < source.width; x++) {
                const Colour& n = current.Fetch(x, y - 1);
                const Colour& s = current.Fetch(x, y + 1);
                const Colour& w = current.Fetch(x - 1, y);
                const Colour& e = current.Fetch(x + 1, y);
                const Colour& stored = source.At(x, y);
                Colour& out = next.At(x, y);
                for (int c = 0; c < 4; c++) {
                    const float average = (n[c] + s[c] + w[c] + e[c]) * 0.25f;
                    out[c] =
                        std::clamp(average, stored[c] - half_width[c], stored[c] + half_width[c]);
                }
            }
        }
        std::swap(current, next);
    }
    return current;
}

} // namespace Pomegrade::Darp
