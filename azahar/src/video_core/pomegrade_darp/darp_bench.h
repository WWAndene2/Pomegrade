// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Thesis s.13: the ground truth is in the games. A texture with mipmaps gives a real pair: its
// level 1 is reconstructed at x2 and compared with its level 0, by DARP and by the classic
// interpolations. Caveat (thesis): mipmaps made by a plain box filter only measure how well the
// filter is inverted.

#include <array>
#include <string_view>
#include "video_core/pomegrade_darp/darp.h"

namespace Pomegrade::Darp {

enum class Method : u32 {
    Darp,
    Nearest,
    Bilinear,
    Bicubic,
    Lanczos,
    Count,
};

std::string_view MethodName(Method method);

struct Scores {
    double psnr = 0;       ///< dB on RGBA, higher is better
    double ssim = 0;       ///< luma, 8x8 windows, 1 = identical
    double edge_error = 0; ///< mean |Sobel magnitude difference| on luma, lower is better
};

/// The scores of each method reconstructing level 1 at x2 against level 0.
std::array<Scores, static_cast<std::size_t>(Method::Count)> EvaluateMipPair(const Texture& level1,
                                                                            const Texture& level0,
                                                                            const Options& options);

/// x2 enlargement by a classic method (the bench's references).
Texture ReferenceUpscale(const Texture& texture, Method method, Wrap wrap_s, Wrap wrap_t);

Scores Compare(const Texture& result, const Texture& truth);

} // namespace Pomegrade::Darp
