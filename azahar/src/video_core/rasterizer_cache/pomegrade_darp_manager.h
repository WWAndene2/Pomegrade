// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Pomegrade: runs DARP (video_core/pomegrade_darp/darp.h) for the rasterizer cache. The thesis'
// execution model (s.11): once per texture, off the emulation thread, with a disk cache. The
// texture first shows with the GPU's fallback upscaler (xBRZ); when its reconstruction is ready,
// the rasterizer cache swaps it in through the custom texture path (an RGBA8 material), as it does
// for texture packs. Results are cached in <cache>/darp/, keyed by a hash of the texture's stored
// bytes, its format, the factor, its wrap modes and Pomegrade::Darp::Version.
// It also gives a texture the Remaster's surface maps (VideoCore::MaterialRecognition, packed in the
// material's normal map: RG the normal, B the height, A the volume), on the reconstruction or, with
// factor 1, on the texture as the game stores it. Those are recomputed when the colour comes from
// the disk cache.

#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>
#include "common/thread_worker.h"
#include "video_core/custom_textures/material.h"
#include "video_core/pomegrade_darp/darp.h"
#include "video_core/pomegrade_darp/darp_bench.h"
#include "video_core/rasterizer_cache/surface_params.h"

namespace Frontend {
class ImageInterface;
}

namespace VideoCore {

struct DarpJob {
    u64 key = 0;
    SurfaceParams params;      ///< level 0 of the texture
    std::vector<u8> encoded;   ///< its bytes as the game stores them
    SurfaceParams mip1_params; ///< level 1, when the game provides it (thesis s.13 bench)
    std::vector<u8> mip1_encoded;
    Pomegrade::Darp::Options options; ///< factor 1: no reconstruction, the surface maps only
    bool surface_maps = false;
};

class DarpManager {
public:
    explicit DarpManager(Frontend::ImageInterface& image_interface);
    ~DarpManager();

    /// The cache key of a texture (thesis s.11).
    static u64 Key(const SurfaceParams& params, std::span<const u8> encoded,
                   const Pomegrade::Darp::Options& options, bool surface_maps);

    /// The DARP equivalent of a texture's format.
    static Pomegrade::Darp::SourceFormat SourceFormat(PixelFormat format);

    /// The reconstruction of key, when it is ready in memory, else nullptr.
    Material* Ready(u64 key);

    /// Queues a reconstruction. Does nothing when key is already queued or ready.
    void Queue(DarpJob&& job);

    /// Keys whose work finished since the last call, with their material (nullptr: DARP left the
    /// texture to the regular path, e.g. a data texture). Emulation thread.
    std::vector<std::pair<u64, Material*>> TakeFinished();

    /// Frees the CPU copy of key's reconstruction once uploaded. The Material stays (surfaces
    /// point to it); a later request reloads it from the disk cache.
    void Release(u64 key);

private:
    enum class State : u32 { Queued, Ready, Released, Failed };

    struct Entry {
        State state = State::Queued;
        std::unique_ptr<CustomTexture> texture;
        std::unique_ptr<CustomTexture> maps; ///< the surface maps, when asked
        std::unique_ptr<Material> material;
    };

    void Run(DarpJob job);
    std::optional<Pomegrade::Darp::Texture> LoadCached(u64 key) const;
    void StoreCached(u64 key, const Pomegrade::Darp::Texture& texture) const;
    void Bench(const DarpJob& job, const Pomegrade::Darp::Texture& level0);

    Frontend::ImageInterface& image_interface;
    std::mutex mutex;
    std::unordered_map<u64, Entry> entries;
    std::vector<u64> finished;

    // thesis s.13: running sums of each method's scores on the games' own mip pairs
    std::mutex bench_mutex;
    std::array<Pomegrade::Darp::Scores, static_cast<std::size_t>(Pomegrade::Darp::Method::Count)>
        bench_sums{};
    u32 bench_pairs = 0;

    // last member: destroyed (threads joined) before what the work uses
    std::unique_ptr<Common::ThreadWorker> worker;
};

} // namespace VideoCore
