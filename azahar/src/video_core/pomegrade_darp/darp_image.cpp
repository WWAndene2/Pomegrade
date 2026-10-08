// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

Image FromTexture(const Texture& texture, Wrap wrap_s, Wrap wrap_t) {
    Image image(static_cast<int>(texture.width), static_cast<int>(texture.height), wrap_s, wrap_t);
    for (std::size_t i = 0; i < image.texels.size(); i++) {
        for (int c = 0; c < 4; c++) {
            image.texels[i][c] = texture.rgba[i * 4 + c] / 255.f;
        }
    }
    return image;
}

Texture ToTexture(const Image& image) {
    Texture texture{static_cast<u32>(image.width), static_cast<u32>(image.height), {}};
    texture.rgba.resize(image.texels.size() * 4);
    for (std::size_t i = 0; i < image.texels.size(); i++) {
        for (int c = 0; c < 4; c++) {
            const float value = std::clamp(image.texels[i][c], 0.f, 1.f);
            texture.rgba[i * 4 + c] = static_cast<u8>(std::lround(value * 255.f));
        }
    }
    return texture;
}

Image BoxDownsample(const Image& image, int factor) {
    Image result(std::max(1, image.width / factor), std::max(1, image.height / factor),
                 image.wrap_s, image.wrap_t);
    const float weight = 1.f / static_cast<float>(factor * factor);
    for (int y = 0; y < result.height; y++) {
        for (int x = 0; x < result.width; x++) {
            Colour sum{};
            for (int dy = 0; dy < factor; dy++) {
                for (int dx = 0; dx < factor; dx++) {
                    const Colour& texel = image.Fetch(x * factor + dx, y * factor + dy);
                    for (int c = 0; c < 4; c++) {
                        sum[c] += texel[c];
                    }
                }
            }
            for (int c = 0; c < 4; c++) {
                result.At(x, y)[c] = sum[c] * weight;
            }
        }
    }
    return result;
}

Colour QuantizationHalfWidth(SourceFormat format) {
    // half the step between two stored values: 0.5 / (2^bits - 1). Channels the format doesn't
    // store (constant) are exact, which an 8-bit half step stands for.
    const auto half = [](int bits) { return 0.5f / static_cast<float>((1 << bits) - 1); };
    const float exact = half(8);
    switch (format) {
    case SourceFormat::RGB5A1:
        return {half(5), half(5), half(5), half(1)};
    case SourceFormat::RGB565:
        return {half(5), half(6), half(5), exact};
    case SourceFormat::RGBA4:
        return {half(4), half(4), half(4), half(4)};
    case SourceFormat::IA4:
    case SourceFormat::I4:
    case SourceFormat::A4:
        return {half(4), half(4), half(4), half(4)};
    case SourceFormat::ETC1:
    case SourceFormat::ETC1A4: {
        // HYPOTHESIS: ETC1 base colours are 4 or 5 bits and its modifiers are coarse; +-4/255 is
        // taken as the colour uncertainty. Its alpha (ETC1A4) is 4 bits.
        const float etc = 4.f / 255.f;
        return {etc, etc, etc, format == SourceFormat::ETC1A4 ? half(4) : exact};
    }
    default:
        return {exact, exact, exact, exact};
    }
}

} // namespace Pomegrade::Darp
