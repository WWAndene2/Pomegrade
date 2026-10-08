// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thesis s.3: what a texture is decides how it is reconstructed. The thresholds are a HYPOTHESIS,
// to calibrate on real games. In doubt the rule is to leave a texture alone, so formats that hold
// data rather than an image are never classified as images.

#include <unordered_set>
#include "video_core/pomegrade_darp/darp_image.h"

namespace Pomegrade::Darp {

Classification Classify(const Texture& texture, SourceFormat format) {
    Classification result;
    if (format == SourceFormat::HILO8 || texture.width == 0 || texture.height == 0) {
        result.colour = TextureClass::Data;
        return result;
    }

    const std::size_t count = static_cast<std::size_t>(texture.width) * texture.height;

    // alpha shape: nearly every texel opaque or transparent, with some of each
    std::size_t binary = 0;
    std::size_t transparent = 0;
    for (std::size_t i = 0; i < count; i++) {
        const u8 alpha = texture.rgba[i * 4 + 3];
        if (alpha <= 15 || alpha >= 240) {
            binary++;
        }
        if (alpha < 128) {
            transparent++;
        }
    }
    result.alpha_mask =
        binary * 100 >= count * 95 && transparent * 100 >= count * 2 && transparent < count;

    if (texture.width <= 8 || texture.height <= 8) {
        result.colour = TextureClass::Small;
        return result;
    }

    // pixel art: few colours in each 8x8 block on average, or few colours overall
    std::unordered_set<u32> all_colours;
    std::size_t block_colours = 0;
    std::size_t blocks = 0;
    for (u32 by = 0; by < texture.height; by += 8) {
        for (u32 bx = 0; bx < texture.width; bx += 8) {
            std::unordered_set<u32> colours;
            for (u32 y = by; y < std::min(by + 8, texture.height); y++) {
                for (u32 x = bx; x < std::min(bx + 8, texture.width); x++) {
                    const u8* texel =
                        &texture.rgba[(static_cast<std::size_t>(y) * texture.width + x) * 4];
                    const u32 colour = texel[0] | texel[1] << 8 | texel[2] << 16 | texel[3] << 24;
                    colours.insert(colour);
                    all_colours.insert(colour);
                }
            }
            block_colours += colours.size();
            blocks++;
        }
    }
    const bool few_overall = all_colours.size() <= 64;
    const bool few_per_block = block_colours <= blocks * 12;
    result.colour =
        few_overall || few_per_block ? TextureClass::PixelArt : TextureClass::Continuous;
    return result;
}

} // namespace Pomegrade::Darp
