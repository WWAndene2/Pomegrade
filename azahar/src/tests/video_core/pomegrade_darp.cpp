// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <set>
#include <catch2/catch_test_macros.hpp>
#include "video_core/pomegrade_darp/darp.h"
#include "video_core/pomegrade_darp/darp_bench.h"
#include "video_core/pomegrade_darp/darp_image.h"

using namespace Pomegrade::Darp;

namespace {

Texture Make(u32 width, u32 height, auto&& colour_at) {
    Texture texture{width, height, std::vector<u8>(width * height * 4)};
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            const std::array<u8, 4> c = colour_at(x, y);
            std::copy(c.begin(), c.end(), texture.rgba.begin() + (y * width + x) * 4);
        }
    }
    return texture;
}

/// A smooth texture with many colours (classified Continuous)
Texture Smooth(u32 size) {
    return Make(size, size, [size](u32 x, u32 y) {
        const double fx = static_cast<double>(x) / size;
        const double fy = static_cast<double>(y) / size;
        return std::array<u8, 4>{static_cast<u8>(127 + 100 * std::sin(fx * 6.28 * 2)),
                                 static_cast<u8>(127 + 100 * std::cos(fy * 6.28 * 3)),
                                 static_cast<u8>(255 * fx * fy), 255};
    });
}

std::array<u8, 4> TexelOf(const Texture& t, u32 x, u32 y) {
    const u8* p = &t.rgba[(y * t.width + x) * 4];
    return {p[0], p[1], p[2], p[3]};
}

} // Anonymous namespace

TEST_CASE("DARP classifies data, pixel art, continuous and alpha masks", "[video_core][darp]") {
    const Texture smooth = Smooth(32);
    REQUIRE(Classify(smooth, SourceFormat::HILO8).colour == TextureClass::Data);
    REQUIRE(Classify(smooth, SourceFormat::RGBA8).colour == TextureClass::Continuous);

    const Texture checker = Make(32, 32, [](u32 x, u32 y) {
        return ((x / 4 + y / 4) % 2) ? std::array<u8, 4>{200, 40, 40, 255}
                                     : std::array<u8, 4>{20, 20, 90, 255};
    });
    REQUIRE(Classify(checker, SourceFormat::RGBA8).colour == TextureClass::PixelArt);
    REQUIRE(Classify(Make(8, 8, [](u32, u32) { return std::array<u8, 4>{1, 2, 3, 255}; }),
                     SourceFormat::RGBA8)
                .colour == TextureClass::Small);

    const Texture leaf = Make(32, 32, [](u32 x, u32 y) {
        const int dx = static_cast<int>(x) - 16, dy = static_cast<int>(y) - 16;
        return std::array<u8, 4>{30, 160, 40, static_cast<u8>(dx * dx + dy * dy < 100 ? 255 : 0)};
    });
    REQUIRE(Classify(leaf, SourceFormat::RGBA8).alpha_mask);
    REQUIRE_FALSE(Classify(smooth, SourceFormat::RGBA8).alpha_mask);
}

TEST_CASE("DARP leaves data textures and bad input to the regular path", "[video_core][darp]") {
    REQUIRE_FALSE(Upscale(Smooth(16), {.factor = 2, .format = SourceFormat::HILO8}).has_value());
    REQUIRE_FALSE(Upscale(Smooth(16), {.factor = 3}).has_value());
    REQUIRE_FALSE(Upscale(Texture{4, 4, {}}, {.factor = 2}).has_value());
}

TEST_CASE("DARP is deterministic and has the requested size", "[video_core][darp]") {
    const Texture source = Smooth(32);
    for (u32 factor : {2u, 4u, 8u, 16u}) {
        const auto a = Upscale(source, {.factor = factor});
        const auto b = Upscale(source, {.factor = factor});
        REQUIRE(a.has_value());
        REQUIRE(a->width == 32 * factor);
        REQUIRE(a->height == 32 * factor);
        REQUIRE(a->rgba == b->rgba);
    }
}

TEST_CASE("DARP's kernel keeps a one-texel line (thesis v1 flaw)", "[video_core][darp]") {
    // grey background, a white vertical line one texel wide: E == W != P must not read as flat
    Image image(8, 8, Wrap::Clamp, Wrap::Clamp);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            const float v = x == 4 ? 1.f : 0.2f;
            image.At(x, y) = {v, v, v, 1.f};
        }
    }
    const Image result = DirectionalKernel(image, 1.f / 255.f);
    for (int y = 0; y < 16; y++) {
        REQUIRE(result.At(8, y)[0] > 0.85f);
        REQUIRE(result.At(9, y)[0] > 0.85f);
        REQUIRE(result.At(6, y)[0] < 0.35f);
    }
}

TEST_CASE("DARP back-projects: reduced again, the result gives the source back",
          "[video_core][darp]") {
    const Texture source = Smooth(32);
    for (u32 factor : {2u, 4u}) {
        const auto result = Upscale(source, {.factor = factor});
        REQUIRE(result.has_value());
        const Texture reduced = ToTexture(BoxDownsample(
            FromTexture(*result, Wrap::Clamp, Wrap::Clamp), static_cast<int>(factor)));
        int worst = 0;
        for (std::size_t i = 0; i < source.rgba.size(); i++) {
            worst = std::max(worst, std::abs(static_cast<int>(reduced.rgba[i]) - source.rgba[i]));
        }
        // the envelope clamp runs last and may pull a few blocks off by a little
        REQUIRE(worst <= 6);
    }
}

TEST_CASE("DARP dequantizes 16-bit staircases into ramps, within their intervals",
          "[video_core][darp]") {
    // a horizontal ramp stored in 5 bits: 8-texel wide steps
    Image source(64, 4, Wrap::Clamp, Wrap::Clamp);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 64; x++) {
            const float v = std::round(x / 8.f) / 31.f;
            source.At(x, y) = {v, v, v, 1.f};
        }
    }
    const Image result = Dequantize(source, SourceFormat::RGB565);
    const float half = 0.5f / 31.f;
    std::set<int> before, after;
    for (int x = 0; x < 64; x++) {
        before.insert(static_cast<int>(std::lround(source.At(x, 1)[0] * 255)));
        after.insert(static_cast<int>(std::lround(result.At(x, 1)[0] * 255)));
        REQUIRE(std::abs(result.At(x, 1)[0] - source.At(x, 1)[0]) <= half + 1e-5f);
        if (x > 0) {
            REQUIRE(result.At(x, 1)[0] >= result.At(x - 1, 1)[0] - 1e-5f); // still a ramp
        }
    }
    REQUIRE(after.size() > before.size() * 2);
}

TEST_CASE("DARP keeps pixel art in its palette", "[video_core][darp]") {
    const Texture checker = Make(32, 32, [](u32 x, u32 y) {
        return ((x / 4 + y / 4) % 2) ? std::array<u8, 4>{200, 40, 40, 255}
                                     : std::array<u8, 4>{20, 20, 90, 255};
    });
    const auto result = Upscale(checker, {.factor = 4});
    REQUIRE(result.has_value());
    std::set<u32> colours;
    for (std::size_t i = 0; i < result->rgba.size(); i += 4) {
        colours.insert(result->rgba[i] | result->rgba[i + 1] << 8 | result->rgba[i + 2] << 16);
    }
    REQUIRE(colours.size() == 2);
}

TEST_CASE("DARP rebuilds alpha shapes sharp", "[video_core][darp]") {
    const Texture leaf = Make(32, 32, [](u32 x, u32 y) {
        const int dx = static_cast<int>(x) - 16, dy = static_cast<int>(y) - 16;
        return std::array<u8, 4>{30, 160, 40, static_cast<u8>(dx * dx + dy * dy < 100 ? 255 : 0)};
    });
    const auto result = Upscale(leaf, {.factor = 8});
    REQUIRE(result.has_value());
    std::size_t partial = 0, opaque = 0;
    for (std::size_t i = 3; i < result->rgba.size(); i += 4) {
        const u8 a = result->rgba[i];
        partial += a > 10 && a < 245;
        opaque += a >= 128;
    }
    std::size_t source_opaque = 0;
    for (std::size_t i = 3; i < leaf.rgba.size(); i += 4) {
        source_opaque += leaf.rgba[i] >= 128;
    }
    const double area = static_cast<double>(opaque) / 64.0; // in source texels (x8 = 64 per texel)
    REQUIRE(std::abs(area - source_opaque) < source_opaque * 0.05); // the disk keeps its size
    REQUIRE(partial < opaque / 10);                                 // a thin edge, not a blur
    // transparent texels took the leaf's colour: no dark fringe
    REQUIRE(TexelOf(*result, 0, 0)[1] > 100);
}

TEST_CASE("DARP follows the wrap mode: a repeating texture has no seam", "[video_core][darp]") {
    const Texture source = Smooth(32);
    // the same texture shifted by 8 texels: with Repeat, the results must be the same, shifted
    const Texture shifted =
        Make(32, 32, [&](u32 x, u32 y) { return TexelOf(source, (x + 8) % 32, y); });
    const Options options{.factor = 2, .wrap_s = Wrap::Repeat, .wrap_t = Wrap::Repeat};
    const auto a = Upscale(source, options);
    const auto b = Upscale(shifted, options);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    for (u32 y = 0; y < 64; y++) {
        for (u32 x = 0; x < 64; x++) {
            REQUIRE(TexelOf(*b, x, y) == TexelOf(*a, (x + 16) % 64, y));
        }
    }
}

TEST_CASE("DARP bench scores a mip pair against the classic methods", "[video_core][darp]") {
    const Texture level0 = Smooth(64);
    const Texture level1 =
        ToTexture(BoxDownsample(FromTexture(level0, Wrap::Clamp, Wrap::Clamp), 2));
    const auto scores = EvaluateMipPair(level1, level0, {.factor = 2});
    const auto& darp = scores[static_cast<std::size_t>(Method::Darp)];
    const auto& nearest = scores[static_cast<std::size_t>(Method::Nearest)];
    REQUIRE(darp.psnr > 20.0);
    REQUIRE(darp.psnr > nearest.psnr);
    REQUIRE(Compare(level0, level0).ssim > 0.999);
}
