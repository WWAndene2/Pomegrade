// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include "video_core/material_recognition.h"

namespace VideoCore::MaterialRecognition {

namespace {

// materials.py's CLASSES (roughness, metalness, relief strength, transmission) and VOLUME, in Class order
constexpr std::array<ClassProperties, ClassCount> PROPERTIES = {{
    {0.05f, 0.0f, 0.0f, 0.85f, 0.0f}, // glass
    {0.75f, 0.0f, 0.4f, 0.0f, 0.6f},  // foliage
    {0.65f, 0.0f, 1.6f, 0.0f, 0.6f},  // wood
    {0.45f, 0.0f, 1.4f, 0.0f, 0.8f},  // tile
    {0.85f, 0.0f, 2.6f, 0.0f, 1.0f},  // stone
    {0.9f, 0.0f, 0.6f, 0.0f, 0.0f},   // plaster
    {0.35f, 0.7f, 0.5f, 0.0f, 0.0f},  // metal
    {0.8f, 0.0f, 0.8f, 0.0f, 0.0f},   // matte
}};

// The reference profiles materials.py measures in ORAS (Littleroot's piece 6 with area pack 8, its REFERENCES regions),
// mean and spread (+0.03) of the five features, in its order: stone, wood, plaster, tile, glass, foliage, metal
struct Profile {
    Class cls;
    float mean[5], spread[5];
};
constexpr std::array<Profile, 7> PROFILES = {{
    {Class::Stone, {0.273550f, 0.305233f, 0.684573f, 0.495454f, 0.451118f}, {0.428363f, 0.120018f, 0.134635f, 0.351559f, 0.274853f}},
    {Class::Wood, {0.570603f, 0.400247f, 0.594339f, 0.312409f, 0.639770f}, {0.148948f, 0.117182f, 0.139682f, 0.230107f, 0.308653f}},
    {Class::Plaster, {0.310902f, 0.232610f, 0.688943f, 0.274387f, 0.584502f}, {0.198363f, 0.112799f, 0.179011f, 0.314570f, 0.380121f}},
    {Class::Tile, {0.371940f, 0.397971f, 0.703232f, 0.286085f, 0.600002f}, {0.184352f, 0.166427f, 0.187708f, 0.249203f, 0.349467f}},
    {Class::Glass, {-0.115713f, -0.042978f, 0.747209f, 0.185711f, 0.795910f}, {0.052185f, 0.035485f, 0.091384f, 0.165585f, 0.192398f}},
    {Class::Foliage, {-0.533939f, 0.463257f, 0.738745f, 0.084048f, 0.245403f}, {0.186586f, 0.080512f, 0.093377f, 0.071761f, 0.156741f}},
    {Class::Metal, {-0.242899f, 0.109995f, 0.421467f, 0.296906f, 0.747463f}, {0.225666f, 0.094345f, 0.145498f, 0.209446f, 0.309753f}},
}};

enum class Mode { Wrap, Reflect, Zero };

struct Image {
    int w = 0, h = 0;
    std::vector<float> v;
    Image() = default;
    Image(int w_, int h_, float fill = 0.0f) : w(w_), h(h_), v(static_cast<size_t>(w_) * h_, fill) {}
    float& at(int x, int y) { return v[static_cast<size_t>(y) * w + x]; }
    float at(int x, int y) const { return v[static_cast<size_t>(y) * w + x]; }
};

// scipy's edge modes: wrap (a b c | a b c), reflect (c b a | a b c | c b a); Zero is handled by the caller
int Index(int i, int n, Mode mode) {
    if (mode == Mode::Wrap) {
        return ((i % n) + n) % n;
    }
    while (i < 0 || i >= n) {
        i = i < 0 ? -i - 1 : 2 * n - i - 1;
    }
    return i;
}

// scipy.ndimage.correlate1d with origin 0: out[i] = sum_k w[k] in[i + k - len / 2], along x (axis 1) or y (axis 0)
Image Correlate1d(const Image& in, const std::vector<float>& w, bool along_x, Mode mode) {
    Image out(in.w, in.h);
    const int r = static_cast<int>(w.size()) / 2, n = along_x ? in.w : in.h;
    for (int y = 0; y < in.h; y++) {
        for (int x = 0; x < in.w; x++) {
            float s = 0.0f;
            for (int k = 0; k < static_cast<int>(w.size()); k++) {
                const int i = Index((along_x ? x : y) + k - r, n, mode);
                s += w[k] * (along_x ? in.at(i, y) : in.at(x, i));
            }
            out.at(x, y) = s;
        }
    }
    return out;
}

Image Uniform(const Image& in, int size, Mode mode) {
    const std::vector<float> w(size, 1.0f / static_cast<float>(size));
    return Correlate1d(Correlate1d(in, w, false, mode), w, true, mode);
}

// scipy.ndimage.sobel(input, axis): [-1 0 1] along the axis, [1 2 1] along the other
Image Sobel(const Image& in, bool along_x, Mode mode) {
    return Correlate1d(Correlate1d(in, {-1.0f, 0.0f, 1.0f}, along_x, mode), {1.0f, 2.0f, 1.0f}, !along_x, mode);
}

// scipy's _gaussian_kernel1d (truncate 4), reversed as gaussian_filter1d correlates it; order 0 or 1
std::vector<float> GaussianKernel(float sigma, int order) {
    const int r = static_cast<int>(4.0f * sigma + 0.5f);
    std::vector<double> phi(2 * r + 1);
    double sum = 0.0;
    for (int x = -r; x <= r; x++) {
        phi[x + r] = std::exp(-0.5 * x * x / (static_cast<double>(sigma) * sigma));
        sum += phi[x + r];
    }
    std::vector<float> w(2 * r + 1);
    for (int x = -r; x <= r; x++) {
        double v = phi[x + r] / sum;
        if (order == 1) {
            v *= -x / (static_cast<double>(sigma) * sigma);
        }
        w[r - x] = static_cast<float>(v); // reversed
    }
    return w;
}

Image Gaussian(const Image& in, float sigma, Mode mode) {
    const auto w = GaussianKernel(sigma, 0);
    return Correlate1d(Correlate1d(in, w, false, mode), w, true, mode);
}

// scipy.ndimage.gaussian_gradient_magnitude
Image GaussianGradientMagnitude(const Image& in, float sigma, Mode mode) {
    const auto g0 = GaussianKernel(sigma, 0), g1 = GaussianKernel(sigma, 1);
    const Image dy = Correlate1d(Correlate1d(in, g1, false, mode), g0, true, mode);
    const Image dx = Correlate1d(Correlate1d(in, g0, false, mode), g1, true, mode);
    Image out(in.w, in.h);
    for (size_t i = 0; i < out.v.size(); i++) {
        out.v[i] = std::sqrt(dx.v[i] * dx.v[i] + dy.v[i] * dy.v[i]);
    }
    return out;
}

Image Median(const Image& in, int size, Mode mode) {
    Image out(in.w, in.h);
    const int r = size / 2;
    std::vector<float> window(static_cast<size_t>(size) * size);
    for (int y = 0; y < in.h; y++) {
        for (int x = 0; x < in.w; x++) {
            size_t k = 0;
            for (int j = -r; j <= r; j++) {
                for (int i = -r; i <= r; i++) {
                    window[k++] = in.at(Index(x + i, in.w, mode), Index(y + j, in.h, mode));
                }
            }
            std::nth_element(window.begin(), window.begin() + window.size() / 2, window.end());
            out.at(x, y) = window[window.size() / 2];
        }
    }
    return out;
}

// scipy.ndimage.binary_opening with ones((s, s)), border 0: the texels covered by an s x s window lying wholly in the set
// (the window at a texel spans offsets -s/2 .. s - 1 - s/2, scipy's origin for an even size)
std::vector<uint8_t> Opening(const std::vector<uint8_t>& set, int w, int h, int s) {
    const int lo = -(s / 2), hi = s - 1 - s / 2;
    std::vector<uint8_t> eroded(set.size(), 0), out(set.size(), 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool all = true;
            for (int j = lo; j <= hi && all; j++) {
                for (int i = lo; i <= hi && all; i++) {
                    const int xx = x + i, yy = y + j;
                    all = xx >= 0 && yy >= 0 && xx < w && yy < h && set[static_cast<size_t>(yy) * w + xx];
                }
            }
            eroded[static_cast<size_t>(y) * w + x] = all;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!eroded[static_cast<size_t>(y) * w + x]) {
                continue;
            }
            for (int j = lo; j <= hi; j++) {
                for (int i = lo; i <= hi; i++) {
                    const int xx = x + i, yy = y + j;
                    if (xx >= 0 && yy >= 0 && xx < w && yy < h) {
                        out[static_cast<size_t>(yy) * w + xx] = 1;
                    }
                }
            }
        }
    }
    return out;
}

// scipy.ndimage.binary_dilation, the default cross, one iteration, border 0
std::vector<uint8_t> DilateCross(const std::vector<uint8_t>& set, int w, int h) {
    std::vector<uint8_t> out(set);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!set[static_cast<size_t>(y) * w + x]) {
                continue;
            }
            if (x > 0) out[static_cast<size_t>(y) * w + x - 1] = 1;
            if (x + 1 < w) out[static_cast<size_t>(y) * w + x + 1] = 1;
            if (y > 0) out[static_cast<size_t>(y - 1) * w + x] = 1;
            if (y + 1 < h) out[static_cast<size_t>(y + 1) * w + x] = 1;
        }
    }
    return out;
}

// scipy.ndimage.distance_transform_edt of the texels not in `zero` (exact Euclidean distance to the nearest texel of
// `zero`), by Felzenszwalb and Huttenlocher's separable squared-distance transform
Image DistanceTransform(const std::vector<uint8_t>& zero, int w, int h) {
    const float inf = 1e20f;
    Image d(w, h);
    for (size_t i = 0; i < d.v.size(); i++) {
        d.v[i] = zero[i] ? 0.0f : inf;
    }
    const auto pass = [](std::vector<float>& f) {
        const int n = static_cast<int>(f.size());
        std::vector<float> out(n), z(n + 1);
        std::vector<int> v(n);
        int k = 0;
        v[0] = 0;
        z[0] = -std::numeric_limits<float>::infinity();
        z[1] = std::numeric_limits<float>::infinity();
        for (int q = 1; q < n; q++) {
            float s;
            while (true) {
                s = ((f[q] + static_cast<float>(q) * q) - (f[v[k]] + static_cast<float>(v[k]) * v[k])) /
                    (2.0f * static_cast<float>(q - v[k]));
                if (s > z[k] || k == 0) {
                    break;
                }
                k--;
            }
            if (s <= z[k]) {
                // k == 0 and the parabola at q is below everywhere
                v[0] = q;
                z[0] = -std::numeric_limits<float>::infinity();
                z[1] = std::numeric_limits<float>::infinity();
                continue;
            }
            k++;
            v[k] = q;
            z[k] = s;
            z[k + 1] = std::numeric_limits<float>::infinity();
        }
        k = 0;
        for (int q = 0; q < n; q++) {
            while (z[k + 1] < static_cast<float>(q)) {
                k++;
            }
            out[q] = static_cast<float>(q - v[k]) * (q - v[k]) + f[v[k]];
        }
        f = out;
    };
    std::vector<float> line;
    for (int x = 0; x < w; x++) {
        line.resize(h);
        for (int y = 0; y < h; y++) line[y] = d.at(x, y);
        pass(line);
        for (int y = 0; y < h; y++) d.at(x, y) = line[y];
    }
    for (int y = 0; y < h; y++) {
        line.resize(w);
        for (int x = 0; x < w; x++) line[x] = d.at(x, y);
        pass(line);
        for (int x = 0; x < w; x++) d.at(x, y) = line[x];
    }
    for (float& v : d.v) {
        v = std::sqrt(v);
    }
    return d;
}

// numpy.percentile (linear interpolation)
float Percentile(std::vector<float> values, float p) {
    std::sort(values.begin(), values.end());
    const float at = p / 100.0f * static_cast<float>(values.size() - 1);
    const size_t i = static_cast<size_t>(at);
    const float t = at - static_cast<float>(i);
    return i + 1 < values.size() ? values[i] * (1.0f - t) + values[i + 1] * t : values[i];
}

uint8_t Byte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f)); // numpy's astype(uint8) after a clip: truncation
}

} // namespace

const ClassProperties& Properties(Class c) {
    return PROPERTIES[static_cast<size_t>(c)];
}

Maps Recognise(const uint8_t* rgba, int w, int h) {
    const size_t n = static_cast<size_t>(w) * h;
    Image r(w, h), g(w, h), b(w, h), a(w, h);
    for (size_t i = 0; i < n; i++) {
        r.v[i] = rgba[i * 4] / 255.0f;
        g.v[i] = rgba[i * 4 + 1] / 255.0f;
        b.v[i] = rgba[i * 4 + 2] / 255.0f;
        a.v[i] = rgba[i * 4 + 3] / 255.0f;
    }
    // features(): the colour on the hue circle scaled by saturation, brightness, contrast, stripes
    Image hx(w, h), hy(w, h), mx(w, h), luma(w, h), luma2(w, h);
    for (size_t i = 0; i < n; i++) {
        const float R = r.v[i], G = g.v[i], B = b.v[i];
        const float hi = std::max({R, G, B}), lo = std::min({R, G, B});
        const float sat = hi > 0.0f ? (hi - lo) / std::max(hi, 1e-6f) : 0.0f;
        const float d = std::max(hi - lo, 1e-6f);
        float hue;
        if (hi == R) {
            hue = std::fmod((G - B) / d, 6.0f);
            if (hue < 0.0f) hue += 6.0f; // numpy's % takes the divisor's sign
        } else if (hi == G) {
            hue = (B - R) / d + 2.0f;
        } else {
            hue = (R - G) / d + 4.0f;
        }
        hue *= 3.14159265358979f / 3.0f;
        hx.v[i] = std::cos(hue) * sat;
        hy.v[i] = std::sin(hue) * sat;
        mx.v[i] = hi;
        luma.v[i] = 0.299f * R + 0.587f * G + 0.114f * B;
        luma2.v[i] = luma.v[i] * luma.v[i];
    }
    const Image m1 = Uniform(luma, 9, Mode::Wrap), m2 = Uniform(luma2, 9, Mode::Wrap);
    const Image gx = Sobel(luma, true, Mode::Wrap), gy = Sobel(luma, false, Mode::Wrap);
    Image xx(w, h), yy(w, h), xy(w, h);
    for (size_t i = 0; i < n; i++) {
        xx.v[i] = gx.v[i] * gx.v[i];
        yy.v[i] = gy.v[i] * gy.v[i];
        xy.v[i] = gx.v[i] * gy.v[i];
    }
    const Image jxx = Uniform(xx, 15, Mode::Wrap), jyy = Uniform(yy, 15, Mode::Wrap), jxy = Uniform(xy, 15, Mode::Wrap);
    Image stdv(w, h), coh(w, h);
    for (size_t i = 0; i < n; i++) {
        stdv.v[i] = std::sqrt(std::max(m2.v[i] - m1.v[i] * m1.v[i], 0.0f));
        const float dxy = jxx.v[i] - jyy.v[i];
        coh.v[i] = std::sqrt(dxy * dxy + 4.0f * jxy.v[i] * jxy.v[i]) / std::max(jxx.v[i] + jyy.v[i], 1e-6f);
    }
    const Image f0 = Uniform(hx, 5, Mode::Wrap), f1 = Uniform(hy, 5, Mode::Wrap), f2 = Uniform(mx, 5, Mode::Wrap);

    // classify(): the nearest profile, matte when far from all or transparent, then a 5 x 5 majority vote
    std::vector<uint8_t> cls(n);
    for (size_t i = 0; i < n; i++) {
        const float F[5] = {f0.v[i], f1.v[i], f2.v[i], stdv.v[i] * 6.0f, coh.v[i]};
        float best = std::numeric_limits<float>::infinity();
        Class c = Class::Matte;
        for (const Profile& p : PROFILES) {
            float dist = 0.0f;
            for (int k = 0; k < 5; k++) {
                const float t = (F[k] - p.mean[k]) / p.spread[k];
                dist += t * t;
            }
            if (dist < best) {
                best = dist;
                c = p.cls;
            }
        }
        if (best > 25.0f || a.v[i] < 0.5f) {
            c = Class::Matte;
        }
        cls[i] = static_cast<uint8_t>(c);
    }
    std::array<Image, ClassCount> votes;
    for (int k = 0; k < ClassCount; k++) {
        Image one(w, h);
        for (size_t i = 0; i < n; i++) one.v[i] = cls[i] == k ? 1.0f : 0.0f;
        votes[k] = Uniform(one, 5, Mode::Wrap);
    }
    for (size_t i = 0; i < n; i++) {
        int best = 0;
        for (int k = 1; k < ClassCount; k++) {
            if (votes[k].v[i] > votes[best].v[i]) best = k;
        }
        cls[i] = a.v[i] < 0.5f ? static_cast<uint8_t>(Class::Matte) : static_cast<uint8_t>(best);
    }

    // run(): ORAS's painted outlines, read as raised
    const Image median = Median(luma, 7, Mode::Wrap);
    std::vector<uint8_t> dark(n), outline(n);
    for (size_t i = 0; i < n; i++) dark[i] = (median.v[i] - luma.v[i]) > 0.08f;
    const std::vector<uint8_t> opened = Opening(dark, w, h, 4);
    for (size_t i = 0; i < n; i++) outline[i] = dark[i] && !opened[i];
    outline = DilateCross(outline, w, h);
    Image surface(w, h), outline_f(w, h);
    for (size_t i = 0; i < n; i++) {
        surface.v[i] = outline[i] ? median.v[i] : luma.v[i];
        outline_f.v[i] = outline[i] ? 1.0f : 0.0f;
    }
    // relief: the texture's own shading as height (high-pass), times the class's relief
    const Image low = Gaussian(surface, 6.0f, Mode::Wrap);
    Image height(w, h);
    for (size_t i = 0; i < n; i++) height.v[i] = surface.v[i] - low.v[i];
    height = Gaussian(height, 0.7f, Mode::Wrap);
    const Image ridge = Gaussian(outline_f, 0.8f, Mode::Wrap);
    for (size_t i = 0; i < n; i++) height.v[i] = height.v[i] * PROPERTIES[cls[i]].relief + ridge.v[i] * 0.06f;
    // no relief along the borders between regions: faded to flat within 3 texels
    std::vector<uint8_t> border(n, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (x > 0 && cls[i] != cls[i - 1]) border[i] = 1;
            if (y > 0 && cls[i] != cls[i - w]) border[i] = 1;
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) border[i] = 1;
        }
    }
    const Image edge = GaussianGradientMagnitude(Gaussian(luma, 2.0f, Mode::Reflect), 1.5f, Mode::Reflect);
    for (size_t i = 0; i < n; i++) border[i] |= edge.v[i] > 0.06f;
    const Image dist = DistanceTransform(border, w, h);
    for (size_t i = 0; i < n; i++) height.v[i] *= std::clamp(dist.v[i] / 3.0f, 0.0f, 1.0f);

    Maps out;
    out.width = w;
    out.height = h;
    out.classes = cls;
    out.outline.resize(n);
    for (size_t i = 0; i < n; i++) out.outline[i] = outline[i] ? 255 : 0;
    // height for parallax: 0 deepest, 1 highest, flat surfaces at 1
    const float top = *std::max_element(height.v.begin(), height.v.end());
    float span = 1e-6f;
    for (float v : height.v) span = std::max(span, top - v);
    out.heights.resize(n);
    for (size_t i = 0; i < n; i++) {
        const float hn = height.v[i] - top;
        out.heights[i] = Byte((1.0f + hn / span * std::min(span / 0.08f, 1.0f)) * 255.0f);
    }
    // volume: the parts standing out of their neighbourhood, on the classes that protrude, scaled to the texture's strong
    // relief (its 95th percentile) with a floor at 40 %
    const Image around = Gaussian(height, 4.0f, Mode::Wrap);
    Image rise(w, h);
    for (size_t i = 0; i < n; i++) rise.v[i] = std::max(height.v[i] - around.v[i], 0.0f);
    rise = Gaussian(rise, 0.6f, Mode::Wrap);
    std::vector<float> positive;
    for (size_t i = 0; i < n; i++) {
        rise.v[i] *= PROPERTIES[cls[i]].volume;
        if (rise.v[i] > 0.0f) positive.push_back(rise.v[i]);
    }
    const float scale = positive.empty() ? 1.0f : Percentile(positive, 95.0f);
    out.volume.resize(n);
    for (size_t i = 0; i < n; i++) {
        const float vol = std::clamp((rise.v[i] / std::max(scale, 1e-6f) - 0.4f) / 0.6f, 0.0f, 1.0f);
        out.volume[i] = Byte(vol * 255.0f);
    }
    // normals from the height
    const Image sx = Sobel(height, true, Mode::Wrap), sy = Sobel(height, false, Mode::Wrap);
    out.normal.resize(n * 3);
    for (size_t i = 0; i < n; i++) {
        float x = -sx.v[i] * 2.0f, y = sy.v[i] * 2.0f, z = 1.0f;
        const float len = std::sqrt(x * x + y * y + z * z);
        x /= len, y /= len, z /= len;
        out.normal[i * 3] = Byte((x * 0.5f + 0.5f) * 255.0f);
        out.normal[i * 3 + 1] = Byte((y * 0.5f + 0.5f) * 255.0f);
        out.normal[i * 3 + 2] = Byte((z * 0.5f + 0.5f) * 255.0f);
    }
    return out;
}

} // namespace VideoCore::MaterialRecognition
