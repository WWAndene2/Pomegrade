// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include "video_core/pomegrade_darp/darp_bench.h"
#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

namespace {

float Cubic(float x) {
    // Keys (1981), a = -0.5
    x = std::abs(x);
    if (x < 1.f) {
        return (1.5f * x - 2.5f) * x * x + 1.f;
    }
    if (x < 2.f) {
        return ((-0.5f * x + 2.5f) * x - 4.f) * x + 2.f;
    }
    return 0.f;
}

float Lanczos3(float x) {
    x = std::abs(x);
    if (x < 1e-5f) {
        return 1.f;
    }
    if (x >= 3.f) {
        return 0.f;
    }
    constexpr float Pi = 3.14159265f;
    const float px = Pi * x;
    return 3.f * std::sin(px) * std::sin(px / 3.f) / (px * px);
}

std::vector<double> Luma(const Texture& texture) {
    std::vector<double> luma(static_cast<std::size_t>(texture.width) * texture.height);
    for (std::size_t i = 0; i < luma.size(); i++) {
        const u8* p = &texture.rgba[i * 4];
        luma[i] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
    }
    return luma;
}

} // Anonymous namespace

std::string_view MethodName(Method method) {
    switch (method) {
    case Method::Darp:
        return "DARP";
    case Method::Nearest:
        return "nearest";
    case Method::Bilinear:
        return "bilinear";
    case Method::Bicubic:
        return "bicubic";
    case Method::Lanczos:
        return "Lanczos";
    default:
        return "?";
    }
}

Texture ReferenceUpscale(const Texture& texture, Method method, Wrap wrap_s, Wrap wrap_t) {
    const Image source = FromTexture(texture, wrap_s, wrap_t);
    Image result(source.width * 2, source.height * 2, wrap_s, wrap_t);
    const int radius = method == Method::Lanczos ? 3 : method == Method::Bicubic ? 2 : 1;
    for (int y = 0; y < result.height; y++) {
        for (int x = 0; x < result.width; x++) {
            const float u = (static_cast<float>(x) + 0.5f) / 2.f - 0.5f;
            const float v = (static_cast<float>(y) + 0.5f) / 2.f - 0.5f;
            Colour& out = result.At(x, y);
            if (method == Method::Nearest) {
                out = source.At(x / 2, y / 2);
                continue;
            }
            const int x0 = static_cast<int>(std::floor(u));
            const int y0 = static_cast<int>(std::floor(v));
            Colour sum{};
            float weight_sum = 0.f;
            for (int j = y0 - radius + 1; j <= y0 + radius; j++) {
                for (int i = x0 - radius + 1; i <= x0 + radius; i++) {
                    const float dx = u - static_cast<float>(i);
                    const float dy = v - static_cast<float>(j);
                    float w;
                    if (method == Method::Bilinear) {
                        w = std::max(0.f, 1.f - std::abs(dx)) * std::max(0.f, 1.f - std::abs(dy));
                    } else if (method == Method::Bicubic) {
                        w = Cubic(dx) * Cubic(dy);
                    } else {
                        w = Lanczos3(dx) * Lanczos3(dy);
                    }
                    const Colour& texel = source.Fetch(i, j);
                    for (int c = 0; c < 4; c++) {
                        sum[c] += texel[c] * w;
                    }
                    weight_sum += w;
                }
            }
            for (int c = 0; c < 4; c++) {
                out[c] = sum[c] / weight_sum;
            }
        }
    }
    return ToTexture(result);
}

Scores Compare(const Texture& result, const Texture& truth) {
    Scores scores;
    if (result.width != truth.width || result.height != truth.height || truth.rgba.empty()) {
        return scores;
    }

    double squared = 0;
    for (std::size_t i = 0; i < truth.rgba.size(); i++) {
        const double d = static_cast<double>(result.rgba[i]) - truth.rgba[i];
        squared += d * d;
    }
    const double mse = squared / static_cast<double>(truth.rgba.size());
    scores.psnr = mse == 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);

    const std::vector<double> a = Luma(result);
    const std::vector<double> b = Luma(truth);
    const int width = static_cast<int>(truth.width);
    const int height = static_cast<int>(truth.height);

    // SSIM on 8x8 windows, averaged (Wang et al., 2004)
    constexpr double C1 = (0.01 * 255) * (0.01 * 255);
    constexpr double C2 = (0.03 * 255) * (0.03 * 255);
    double ssim_sum = 0;
    int windows = 0;
    for (int wy = 0; wy + 8 <= height; wy += 4) {
        for (int wx = 0; wx + 8 <= width; wx += 4) {
            double ma = 0, mb = 0;
            for (int y = wy; y < wy + 8; y++) {
                for (int x = wx; x < wx + 8; x++) {
                    ma += a[y * width + x];
                    mb += b[y * width + x];
                }
            }
            ma /= 64;
            mb /= 64;
            double va = 0, vb = 0, cov = 0;
            for (int y = wy; y < wy + 8; y++) {
                for (int x = wx; x < wx + 8; x++) {
                    const double da = a[y * width + x] - ma;
                    const double db = b[y * width + x] - mb;
                    va += da * da;
                    vb += db * db;
                    cov += da * db;
                }
            }
            va /= 63;
            vb /= 63;
            cov /= 63;
            ssim_sum +=
                ((2 * ma * mb + C1) * (2 * cov + C2)) / ((ma * ma + mb * mb + C1) * (va + vb + C2));
            windows++;
        }
    }
    scores.ssim = windows ? ssim_sum / windows : 1.0;

    const auto sobel = [&](const std::vector<double>& l, int x, int y) {
        const auto at = [&](int px, int py) {
            return l[std::clamp(py, 0, height - 1) * width + std::clamp(px, 0, width - 1)];
        };
        const double gx = at(x + 1, y - 1) + 2 * at(x + 1, y) + at(x + 1, y + 1) -
                          at(x - 1, y - 1) - 2 * at(x - 1, y) - at(x - 1, y + 1);
        const double gy = at(x - 1, y + 1) + 2 * at(x, y + 1) + at(x + 1, y + 1) -
                          at(x - 1, y - 1) - 2 * at(x, y - 1) - at(x + 1, y - 1);
        return std::sqrt(gx * gx + gy * gy);
    };
    double edge = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            edge += std::abs(sobel(a, x, y) - sobel(b, x, y));
        }
    }
    scores.edge_error = edge / (static_cast<double>(width) * height);
    return scores;
}

std::array<Scores, static_cast<std::size_t>(Method::Count)> EvaluateMipPair(
    const Texture& level1, const Texture& level0, const Options& options) {
    std::array<Scores, static_cast<std::size_t>(Method::Count)> scores{};
    Options x2 = options;
    x2.factor = 2;
    if (const auto darp = Upscale(level1, x2)) {
        scores[static_cast<std::size_t>(Method::Darp)] = Compare(*darp, level0);
    }
    for (Method method : {Method::Nearest, Method::Bilinear, Method::Bicubic, Method::Lanczos}) {
        scores[static_cast<std::size_t>(method)] =
            Compare(ReferenceUpscale(level1, method, options.wrap_s, options.wrap_t), level0);
    }
    return scores;
}

} // namespace Pomegrade::Darp
