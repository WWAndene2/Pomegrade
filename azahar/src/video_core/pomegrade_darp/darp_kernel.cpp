// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.5: the x2 directional kernel. Each source texel P gives four output texels, one per
// quadrant. For a quadrant, H and V are P's horizontal and vertical neighbours on that side and D
// the diagonal one. Candidates, weighted by exp(-lambda * crossing / sigma):
//   C_h = (3P + H) / 4        crossing |P - H|
//   C_v = (3P + V) / 4        crossing |P - V|
//   C_d = (2P + H + V) / 4    crossing max(|P - H|, |P - V|, |P - D|)
//   C_0 = P                   constant penalty kappa (replication only wins at real edges)
//   C_c = (H + V) / 2         corner, only on a staircase corner: H and V alike, the far sides
//                             (H' opposite H, V' opposite V) different from them (the Scale2x /
//                             EPX rule). Added while implementing: every other candidate is at
//                             least half P, so without it a diagonal edge stays a staircase and
//                             pixel art (snapped to its palette) came out as nearest neighbour.
// sigma is the local spread of colours, so lambda doesn't depend on the texture's contrast.
// A texel whose gradients (three-point, including P: a one-texel line is never flat) all stay
// under the format's quantization step is flat and simply replicated.

#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

namespace {

constexpr float Lambda = 4.f;
constexpr float Kappa = 0.15f;
constexpr float MinimumSigma = 2.f / 255.f;
constexpr float CornerBoost = 4.f; ///< a staircase corner outweighs the candidates leaning to P

} // Anonymous namespace

Image DirectionalKernel(const Image& image, float flat_threshold) {
    Image result(image.width * 2, image.height * 2, image.wrap_s, image.wrap_t);
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            const Colour& p = image.At(x, y);

            // three-point gradients (s.5.1)
            const auto pair = [&](int ax, int ay, int bx, int by) {
                return std::max(Distance(p, image.Fetch(ax, ay)), Distance(p, image.Fetch(bx, by)));
            };
            const float gx = pair(x - 1, y, x + 1, y);
            const float gy = pair(x, y - 1, x, y + 1);
            const float gd1 = pair(x - 1, y - 1, x + 1, y + 1);
            const float gd2 = pair(x + 1, y - 1, x - 1, y + 1);
            const bool flat = std::max({gx, gy, gd1, gd2}) < flat_threshold;

            // local spread: mean distance to the 8 neighbours
            float sigma = 0.f;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    sigma += Distance(p, image.Fetch(x + dx, y + dy));
                }
            }
            sigma = std::max(sigma / 8.f, MinimumSigma);

            for (int qy = 0; qy < 2; qy++) {
                for (int qx = 0; qx < 2; qx++) {
                    Colour& out = result.At(x * 2 + qx, y * 2 + qy);
                    if (flat) {
                        out = p;
                        continue;
                    }
                    const int sx = qx == 0 ? -1 : 1;
                    const int sy = qy == 0 ? -1 : 1;
                    const Colour& h = image.Fetch(x + sx, y);
                    const Colour& v = image.Fetch(x, y + sy);
                    const Colour& d = image.Fetch(x + sx, y + sy);
                    const float ph = Distance(p, h);
                    const float pv = Distance(p, v);
                    const float pd = Distance(p, d);

                    const float w_h = std::exp(-Lambda * ph / sigma);
                    const float w_v = std::exp(-Lambda * pv / sigma);
                    const float w_d = std::exp(-Lambda * std::max({ph, pv, pd}) / sigma);
                    const float w_0 = std::exp(-Lambda * Kappa);

                    // staircase corner (Scale2x condition, with tolerances)
                    const float hv = Distance(h, v);
                    const bool corner = hv < flat_threshold + sigma * 0.25f &&
                                        ph > flat_threshold &&
                                        Distance(h, image.Fetch(x - sx, y)) > flat_threshold &&
                                        Distance(v, image.Fetch(x, y - sy)) > flat_threshold;
                    const float w_c = corner ? CornerBoost * std::exp(-Lambda * hv / sigma) : 0.f;

                    const float total = w_h + w_v + w_d + w_0 + w_c;
                    for (int c = 0; c < 4; c++) {
                        const float c_h = (3.f * p[c] + h[c]) * 0.25f;
                        const float c_v = (3.f * p[c] + v[c]) * 0.25f;
                        const float c_d = (2.f * p[c] + h[c] + v[c]) * 0.25f;
                        const float c_c = (h[c] + v[c]) * 0.5f;
                        out[c] =
                            (w_h * c_h + w_v * c_v + w_d * c_d + w_0 * p[c] + w_c * c_c) / total;
                    }
                }
            }
        }
    }
    return result;
}

} // namespace Pomegrade::Darp
