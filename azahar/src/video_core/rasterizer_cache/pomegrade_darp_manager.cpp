// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

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

/// MaterialRecognition's maps of an RGBA8 texture, packed as the shader reads them
std::vector<u8> SurfaceMaps(const Darp::Texture& texture) {
    const auto maps = VideoCore::MaterialRecognition::Recognise(
        texture.rgba.data(), static_cast<int>(texture.width), static_cast<int>(texture.height));
    std::vector<u8> packed(texture.rgba.size());
    for (std::size_t i = 0; i < maps.heights.size(); i++) {
        packed[i * 4 + 0] = maps.normal[i * 3 + 0];
        packed[i * 4 + 1] = maps.normal[i * 3 + 1];
        packed[i * 4 + 2] = maps.heights[i];
        packed[i * 4 + 3] = maps.volume[i];
    }
    return packed;
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
    if (result && job.surface_maps) {
        maps = SurfaceMaps(*result);
    }

    std::scoped_lock lock{mutex};
    Entry& entry = entries.at(job.key);
    if (result) {
        entry.texture->data = std::move(result->rgba);
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
