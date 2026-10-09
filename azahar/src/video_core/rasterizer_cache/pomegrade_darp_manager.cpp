// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "common/zstd_compression.h"
#include "video_core/material_recognition.h"
#include "video_core/rasterizer_cache/pomegrade_darp_manager.h"
#include "video_core/rasterizer_cache/utils.h"

namespace VideoCore {

namespace Darp = Pomegrade::Darp;

namespace {

constexpr u32 CacheMagic = 0x32505244; // "DRP2"
constexpr u32 BenchReportEvery = 25;

std::string CachePath(u64 key) {
    return fmt::format("{}darp/{:016X}.bin", FileUtil::GetUserPath(FileUtil::UserPath::CacheDir),
                       key);
}

/// The texture as RGBA8, rows in the order the GPU upload expects (as the texture dumper does).
Darp::Texture Decode(const SurfaceParams& params, std::span<u8> encoded) {
    Darp::Texture texture{params.width, params.height, {}};
    texture.rgba.resize(static_cast<std::size_t>(params.width) * params.height * 4);
    DecodeTexture(params, params.addr, params.end, encoded, texture.rgba,
                  params.type == SurfaceType::Color);
    return texture;
}

/// Box mean of a single-channel image, radius r, edges clamped (two passes of running sums)
std::vector<float> BoxMean(const std::vector<float>& v, int w, int h, int r) {
    std::vector<float> tmp(v.size()), out(v.size());
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int k = -r; k <= r; k++) {
                sum += v[static_cast<std::size_t>(y) * w + std::clamp(x + k, 0, w - 1)];
            }
            tmp[static_cast<std::size_t>(y) * w + x] = sum / (2 * r + 1);
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int k = -r; k <= r; k++) {
                sum += tmp[static_cast<std::size_t>(std::clamp(y + k, 0, h - 1)) * w + x];
            }
            out[static_cast<std::size_t>(y) * w + x] = sum / (2 * r + 1);
        }
    }
    return out;
}

/// MaterialRecognition's maps of an RGBA8 texture, packed as the shader reads them (RG the normal,
/// B the height, A the volume up to 254; 255 marks glass, for the windows' interior). Also lifts the
/// texture's painted shadows (in place): the path tracer's light replaces them. A painted shadow is a
/// broad, smooth area darker than its surroundings, never a line: the luminance over r 4 against r 16,
/// only where the texture is smooth (its r 2 mean within 6 % of the r 4 one, the texel within 12 % of
/// its r 2 mean), lifted up to 1.3x, half way, off the outlines and the glass. The first rule (r 2
/// against r 8, 9 October) lifted the grass's painted blades as well and flattened the textures.
std::vector<u8> SurfaceMaps(Darp::Texture& texture) {
    const int w = static_cast<int>(texture.width), h = static_cast<int>(texture.height);
    const auto maps = VideoCore::MaterialRecognition::Recognise(texture.rgba.data(), w, h);
    const std::size_t n = maps.heights.size();
    std::vector<float> luma(n);
    for (std::size_t i = 0; i < n; i++) {
        luma[i] = 0.299f * texture.rgba[i * 4] + 0.587f * texture.rgba[i * 4 + 1] +
                  0.114f * texture.rgba[i * 4 + 2];
    }
    const std::vector<float> fine = BoxMean(luma, w, h, 2), near_ = BoxMean(luma, w, h, 4),
                             wide = BoxMean(luma, w, h, 16);
    // glass only in coherent areas: 80 % of the texel's 5 x 5 neighbourhood glass. The recognition also classes scattered
    // texels of grass and roofs as glass, which would have shown the window's room in specks (seen on screen, 9 October)
    std::vector<float> glass_map(n);
    for (std::size_t i = 0; i < n; i++) {
        glass_map[i] = maps.classes[i] == static_cast<u8>(VideoCore::MaterialRecognition::Class::Glass) ? 1.0f : 0.0f;
    }
    const std::vector<float> glass_share = BoxMean(glass_map, w, h, 2);
    std::vector<u8> packed(texture.rgba.size());
    for (std::size_t i = 0; i < n; i++) {
        const bool glass = glass_map[i] > 0.5f && glass_share[i] >= 0.8f;
        packed[i * 4 + 0] = maps.normal[i * 3 + 0];
        packed[i * 4 + 1] = maps.normal[i * 3 + 1];
        packed[i * 4 + 2] = maps.heights[i];
        packed[i * 4 + 3] = glass ? 255 : std::min<u8>(maps.volume[i], 254);
        if (glass || maps.outline[i] || near_[i] < 1.0f ||
            std::abs(fine[i] - near_[i]) > 0.06f * near_[i] || std::abs(luma[i] - fine[i]) > 0.12f * fine[i]) {
            continue;
        }
        const float lift = 1.0f + 0.5f * (std::clamp(wide[i] / near_[i], 1.0f, 1.3f) - 1.0f);
        for (int c = 0; c < 3; c++) {
            texture.rgba[i * 4 + c] =
                static_cast<u8>(std::min(255.0f, texture.rgba[i * 4 + c] * lift + 0.5f));
        }
    }
    return packed;
}

/// Whether a texture is a painted shadow: the texels it shows (alpha over 1/16) dark (mean luminance under 0.25) and of
/// nearly one colour (spread under 0.06), the shape in a varying alpha (a quarter of them or more translucent). A
/// texture with a painted outline or any drawing in colour fails the uniformity
bool IsShadowDecal(const Darp::Texture& texture) {
    double sum = 0, sum2 = 0, shown = 0, translucent = 0;
    const std::size_t n = texture.rgba.size() / 4;
    for (std::size_t i = 0; i < n; i++) {
        const u8* t = &texture.rgba[i * 4];
        if (t[3] < 16) {
            continue;
        }
        const double l = (0.299 * t[0] + 0.587 * t[1] + 0.114 * t[2]) / 255.0;
        sum += l;
        sum2 += l * l;
        shown++;
        translucent += t[3] < 224 ? 1 : 0;
    }
    if (shown < n * 0.05) {
        return false;
    }
    const double mean = sum / shown, spread = std::sqrt(std::max(0.0, sum2 / shown - mean * mean));
    return mean < 0.25 && spread < 0.06 && translucent >= shown * 0.25;
}

} // Anonymous namespace

DarpManager::DarpManager(Frontend::ImageInterface& image_interface_)
    : image_interface{image_interface_},
      // one thread: bounds the memory (a 2048x2048 reconstruction holds ~100 MB while it runs)
      // and leaves the other cores to the emulation
      worker{std::make_unique<Common::ThreadWorker>(1, "Pomegrade DARP")} {
    FileUtil::CreateFullPath(
        fmt::format("{}darp/", FileUtil::GetUserPath(FileUtil::UserPath::CacheDir)));
}

DarpManager::~DarpManager() = default;

u64 DarpManager::Key(const SurfaceParams& params, std::span<const u8> encoded,
                     const Darp::Options& options, bool surface_maps) {
    const std::array<u32, 8> description = {
        params.width,
        params.height,
        static_cast<u32>(params.pixel_format),
        options.factor,
        static_cast<u32>(options.wrap_s),
        static_cast<u32>(options.wrap_t),
        Darp::Version,
        surface_maps ? 1u : 0u,
    };
    const u64 data = Common::ComputeHash64(encoded.data(), encoded.size());
    const u64 shape = Common::ComputeHash64(description.data(), sizeof(description));
    return data ^ (shape + 0x9E3779B97F4A7C15ULL + (data << 6) + (data >> 2));
}

Darp::SourceFormat DarpManager::SourceFormat(PixelFormat format) {
    switch (format) {
    case PixelFormat::RGBA8:
        return Darp::SourceFormat::RGBA8;
    case PixelFormat::RGB8:
        return Darp::SourceFormat::RGB8;
    case PixelFormat::RGB5A1:
        return Darp::SourceFormat::RGB5A1;
    case PixelFormat::RGB565:
        return Darp::SourceFormat::RGB565;
    case PixelFormat::RGBA4:
        return Darp::SourceFormat::RGBA4;
    case PixelFormat::IA8:
        return Darp::SourceFormat::IA8;
    case PixelFormat::I8:
        return Darp::SourceFormat::I8;
    case PixelFormat::A8:
        return Darp::SourceFormat::A8;
    case PixelFormat::IA4:
        return Darp::SourceFormat::IA4;
    case PixelFormat::I4:
        return Darp::SourceFormat::I4;
    case PixelFormat::A4:
        return Darp::SourceFormat::A4;
    case PixelFormat::ETC1:
        return Darp::SourceFormat::ETC1;
    case PixelFormat::ETC1A4:
        return Darp::SourceFormat::ETC1A4;
    case PixelFormat::RG8: // HILO8: normal maps and other data
    default:
        return Darp::SourceFormat::HILO8;
    }
}

Material* DarpManager::Ready(u64 key) {
    std::scoped_lock lock{mutex};
    const auto it = entries.find(key);
    return it != entries.end() && it->second.state == State::Ready ? it->second.material.get()
                                                                   : nullptr;
}

void DarpManager::Queue(DarpJob&& job) {
    {
        std::scoped_lock lock{mutex};
        Entry& entry = entries[job.key];
        if (entry.material && entry.state != State::Released) {
            return; // queued, ready or failed
        }
        if (!entry.material) {
            entry.texture = std::make_unique<CustomTexture>(image_interface);
            entry.material = std::make_unique<Material>();
            entry.material->format = CustomPixelFormat::RGBA8;
            entry.material->hash = job.key;
            entry.material->textures = {};
            entry.material->textures[0] = entry.texture.get();
            entry.texture->format = CustomPixelFormat::RGBA8;
            entry.texture->type = MapType::Color;
            if (job.surface_maps) {
                entry.maps = std::make_unique<CustomTexture>(image_interface);
                entry.maps->format = CustomPixelFormat::RGBA8;
                entry.maps->type = MapType::Normal;
                entry.material->textures[1] = entry.maps.get();
                entry.material->pomegrade_surface = true;
            }
        }
        entry.state = State::Queued;
    }
    worker->QueueWork([this, job = std::move(job)]() mutable { Run(std::move(job)); });
}

std::vector<std::pair<u64, Material*>> DarpManager::TakeFinished() {
    std::scoped_lock lock{mutex};
    std::vector<std::pair<u64, Material*>> result;
    result.reserve(finished.size());
    for (const u64 key : finished) {
        const Entry& entry = entries.at(key);
        result.emplace_back(key, entry.state == State::Ready ? entry.material.get() : nullptr);
    }
    finished.clear();
    return result;
}

void DarpManager::Release(u64 key) {
    std::scoped_lock lock{mutex};
    const auto it = entries.find(key);
    if (it == entries.end() || it->second.state != State::Ready) {
        return;
    }
    it->second.state = State::Released;
    it->second.texture->data = {};
    if (it->second.maps) {
        it->second.maps->data = {};
    }
}

void DarpManager::Run(DarpJob job) {
    const bool reconstruct = job.options.factor >= 2;
    std::optional<Darp::Texture> result =
        job.options.format == Darp::SourceFormat::HILO8 || !reconstruct ? std::nullopt
                                                                        : LoadCached(job.key);
    if (!reconstruct && job.options.format != Darp::SourceFormat::HILO8) {
        result = Decode(job.params, job.encoded);
    } else if (!result && job.options.format != Darp::SourceFormat::HILO8) {
        const Darp::Texture level0 = Decode(job.params, job.encoded);
        result = Darp::Upscale(level0, job.options);
        if (result) {
            StoreCached(job.key, *result);
            if (!job.mip1_encoded.empty()) {
                Bench(job, level0);
            }
        }
    }

    std::vector<u8> maps;
    bool shadow_decal = false;
    if (result && job.surface_maps) {
        shadow_decal = IsShadowDecal(*result);
        maps = SurfaceMaps(*result);
    }

    std::scoped_lock lock{mutex};
    Entry& entry = entries.at(job.key);
    if (result) {
        entry.texture->data = std::move(result->rgba);
        entry.material->pomegrade_shadow_decal = shadow_decal;
        if (entry.maps) {
            entry.maps->data = std::move(maps);
        }
        // a reload (after Release) has the same size: surfaces read it without the lock, so it is
        // only written the first time
        if (entry.material->state != DecodeState::Decoded) {
            entry.texture->width = result->width;
            entry.texture->height = result->height;
            if (entry.maps) {
                entry.maps->width = result->width;
                entry.maps->height = result->height;
            }
            entry.material->width = result->width;
            entry.material->height = result->height;
            entry.material->size = entry.texture->data.size();
            entry.material->state = DecodeState::Decoded;
        }
        entry.state = State::Ready;
    } else {
        entry.state = State::Failed;
    }
    finished.push_back(job.key);
}

std::optional<Darp::Texture> DarpManager::LoadCached(u64 key) const {
    FileUtil::IOFile file(CachePath(key), "rb");
    if (!file.IsOpen()) {
        return std::nullopt;
    }
    std::array<u32, 3> header{};
    if (file.ReadArray(header.data(), header.size()) != header.size() || header[0] != CacheMagic) {
        return std::nullopt;
    }
    std::vector<u8> compressed(file.GetSize() - sizeof(header));
    if (file.ReadBytes(compressed.data(), compressed.size()) != compressed.size()) {
        return std::nullopt;
    }
    Darp::Texture texture{header[1], header[2],
                          Common::Compression::DecompressDataZSTD(compressed)};
    if (texture.rgba.size() != static_cast<std::size_t>(texture.width) * texture.height * 4) {
        return std::nullopt;
    }
    return texture;
}

void DarpManager::StoreCached(u64 key, const Darp::Texture& texture) const {
    const std::vector<u8> compressed = Common::Compression::CompressDataZSTDDefault(texture.rgba);
    const std::string path = CachePath(key);
    const std::string temporary = path + ".tmp";
    {
        FileUtil::IOFile file(temporary, "wb");
        if (!file.IsOpen()) {
            return;
        }
        const std::array<u32, 3> header = {CacheMagic, texture.width, texture.height};
        file.WriteArray(header.data(), header.size());
        file.WriteBytes(compressed.data(), compressed.size());
    }
    // renamed once complete, so a crash never leaves a truncated cache file under the real name
    FileUtil::Rename(temporary, path);
}

void DarpManager::Bench(const DarpJob& job, const Darp::Texture& level0) {
    std::vector<u8> mip1_encoded = job.mip1_encoded;
    const Darp::Texture level1 = Decode(job.mip1_params, mip1_encoded);
    if (level1.width * 2 != level0.width || level1.height * 2 != level0.height) {
        return;
    }
    const auto scores = Darp::EvaluateMipPair(level1, level0, job.options);

    std::scoped_lock lock{bench_mutex};
    for (std::size_t i = 0; i < scores.size(); i++) {
        bench_sums[i].psnr += scores[i].psnr;
        bench_sums[i].ssim += scores[i].ssim;
        bench_sums[i].edge_error += scores[i].edge_error;
    }
    if (++bench_pairs % BenchReportEvery != 0) {
        return;
    }
    std::string report;
    for (std::size_t i = 0; i < bench_sums.size(); i++) {
        report += fmt::format(" | {} PSNR {:.2f} SSIM {:.4f} edge {:.2f}",
                              Darp::MethodName(static_cast<Darp::Method>(i)),
                              bench_sums[i].psnr / bench_pairs, bench_sums[i].ssim / bench_pairs,
                              bench_sums[i].edge_error / bench_pairs);
    }
    LOG_INFO(Render, "DARP bench on {} mip pairs of this game{}", bench_pairs, report);
}

} // namespace VideoCore
