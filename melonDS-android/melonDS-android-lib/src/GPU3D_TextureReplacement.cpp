#include "GPU3D_TextureReplacement.h"
#include "Platform.h"

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_DEPRECATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

std::mutex TextureReplacement::ConfigMutex;
TextureReplacementConfig TextureReplacement::SharedConfig;
std::string TextureReplacement::SharedGameCode;
u32 TextureReplacement::SharedGeneration = 1;

void TextureReplacement::SetConfig(const TextureReplacementConfig& config)
{
    std::lock_guard<std::mutex> lock(ConfigMutex);
    SharedConfig = config;
    SharedGeneration++;
}

void TextureReplacement::SetGameCode(const std::string& gameCode)
{
    std::lock_guard<std::mutex> lock(ConfigMutex);
    SharedGameCode = gameCode;
    SharedGeneration++;
}

static bool IsSafeGameCode(const std::string& code)
{
    if (code.empty())
        return false;
    for (char c : code)
    {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

void TextureReplacement::Refresh()
{
    std::string gameCode;
    {
        std::lock_guard<std::mutex> lock(ConfigMutex);
        if (SeenGeneration == SharedGeneration)
            return;
        SeenGeneration = SharedGeneration;
        Config = SharedConfig;
        gameCode = SharedGameCode;
    }

    // loads queued for the previous settings or game are dropped; one already
    // being decoded is discarded when it comes back (TakeLoaded)
    {
        std::lock_guard<std::mutex> lock(WorkerMutex);
        Queue.clear();
        Results.clear();
    }
    Pending.clear();

    Index.clear();
    Dumped.clear();
    Rejected.clear();
    LRU.clear();
    MemoryCache.clear();
    MemoryCacheSize = 0;
    GameDir.clear();

    if (Config.RootDir.empty() || !IsSafeGameCode(gameCode) || !(Config.Replace || Config.Dump))
        return;

    GameDir = (fs::path(Config.RootDir) / gameCode).string();
    IndexDirectory();
}

void TextureReplacement::IndexDirectory()
{
    std::error_code ec;
    fs::path gameDir(GameDir);
    fs::path dumpDir = gameDir / "dump";

    if (Config.Dump)
    {
        fs::create_directories(dumpDir, ec);
        if (ec)
            Log(LogLevel::Warn, "TextureReplacement: cannot create %s: %s\n", dumpDir.string().c_str(), ec.message().c_str());

        for (auto it = fs::directory_iterator(dumpDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            if (it->path().extension() == ".png")
                Dumped.insert(it->path().stem().string());
        }
        ec.clear();
    }

    if (Config.Replace && fs::is_directory(gameDir, ec))
    {
        auto opts = fs::directory_options::skip_permission_denied;
        for (auto it = fs::recursive_directory_iterator(gameDir, opts, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (it->is_directory(ec) && it->path().filename() == "dump")
            {
                it.disable_recursion_pending();
                continue;
            }
            const fs::path& p = it->path();
            if (p.extension() == ".png" && p.stem().string().rfind("tex_", 0) == 0)
                Index[p.stem().string()] = p.string();
        }
        if (ec)
            Log(LogLevel::Warn, "TextureReplacement: error while scanning %s: %s\n", GameDir.c_str(), ec.message().c_str());
    }

    Log(LogLevel::Info, "TextureReplacement: %s, %zu replacement(s), dump %s\n",
        GameDir.c_str(), Index.size(), Config.Dump ? "on" : "off");
}

bool TextureReplacement::Active()
{
    Refresh();
    TakeLoaded();
    return !GameDir.empty();
}

TextureReplacement::~TextureReplacement()
{
    StopWorker();
}

void TextureReplacement::StopWorker()
{
    {
        std::lock_guard<std::mutex> lock(WorkerMutex);
        WorkerStop = true;
    }
    WorkerWake.notify_all();
    if (Worker.joinable())
        Worker.join();
}

void TextureReplacement::WorkerLoop()
{
    std::unique_lock<std::mutex> lock(WorkerMutex);
    for (;;)
    {
        WorkerWake.wait(lock, [this] { return WorkerStop || !Queue.empty(); });
        if (WorkerStop)
            return;
        LoadRequest request = std::move(Queue.front());
        Queue.pop_front();
        WorkerBusy = true;
        lock.unlock();

        LoadResult result{request.Generation, request.CacheKey, false, {}};
        result.Ok = Decode(request.Path, request.Width, request.Height, request.BinaryAlpha, request.MaxSize, result.Texture);

        lock.lock();
        Results.push_back(std::move(result));
        WorkerBusy = false;
        WorkerIdle.notify_all();
    }
}

void TextureReplacement::WaitForLoads()
{
    std::unique_lock<std::mutex> lock(WorkerMutex);
    WorkerIdle.wait(lock, [this] { return Queue.empty() && !WorkerBusy; });
}

// decoded replacements go to the memory cache (or the rejected set), on the
// render thread
void TextureReplacement::TakeLoaded()
{
    std::vector<LoadResult> results;
    {
        std::lock_guard<std::mutex> lock(WorkerMutex);
        results.swap(Results);
    }
    bool any = false;
    for (LoadResult& result : results)
    {
        if (result.Generation != SeenGeneration)
            continue;
        Pending.erase(result.CacheKey);
        if (result.Ok)
            Store(result.CacheKey, std::move(result.Texture));
        else
            Rejected.insert(result.CacheKey);
        any = true;
    }
    if (any)
        LoadedCount++;
}

u64 TextureReplacement::HashDecoded(const u32* data, u32 width, u32 height)
{
    return XXH64(data, (size_t)width * height * sizeof(u32), ((u64)width << 32) | height);
}

std::string TextureReplacement::TextureName(u64 hash, u32 width, u32 height)
{
    char name[64];
    snprintf(name, sizeof(name), "tex_%ux%u_%016" PRIx64, width, height, hash);
    return name;
}

// RGB6A5 is how the texture cache stores texels: one byte per channel,
// colour channels 0-63, alpha 0-31.
void TextureReplacement::ConvertToRGB6A5(std::vector<u32>& data)
{
    for (u32& c : data)
    {
        u32 a = ((c >> 24) * 31 + 127) / 255;
        if (a == 0)
            c = 0;
        else
            c = ((c & 0xFF) >> 2) | (((c >> 8) & 0xFF) >> 2 << 8) | (((c >> 16) & 0xFF) >> 2 << 16) | (a << 24);
    }
}

static inline void RGB6A5ToRGBA8(u32 c, u8* p)
{
    u32 r = c & 0x3F, g = (c >> 8) & 0x3F, b = (c >> 16) & 0x3F, a = (c >> 24) & 0x1F;
    p[0] = (r << 2) | (r >> 4);
    p[1] = (g << 2) | (g >> 4);
    p[2] = (b << 2) | (b >> 4);
    p[3] = (a * 255 + 15) / 31;
}

bool TextureReplacement::Lookup(u64 hash, u32 width, u32 height, bool binaryAlpha, u32 maxSize,
                                std::vector<u32>& out, u32& outWidth, u32& outHeight, bool* pending)
{
    if (pending)
        *pending = false;
    if (!Config.Replace || Index.empty())
        return false;

    std::string name = TextureName(hash, width, height);
    auto file = Index.find(name);
    if (file == Index.end())
        return false;

    std::string cacheKey = name + (binaryAlpha ? "/b" : "/a");

    auto cached = MemoryCache.find(cacheKey);
    if (cached == MemoryCache.end())
    {
        if (Rejected.count(cacheKey))
            return false;

        if (Config.Background)
        {
            if (pending)
                *pending = true;
            if (!Pending.insert(cacheKey).second)
                return false; // already queued
            {
                std::lock_guard<std::mutex> lock(WorkerMutex);
                if (!Worker.joinable())
                {
                    WorkerStop = false;
                    Worker = std::thread(&TextureReplacement::WorkerLoop, this);
                }
                Queue.push_back(LoadRequest{SeenGeneration, cacheKey, file->second, width, height, binaryAlpha, maxSize});
            }
            WorkerWake.notify_one();
            return false;
        }

        CachedTexture tex;
        if (!Decode(file->second, width, height, binaryAlpha, maxSize, tex))
        {
            Rejected.insert(cacheKey);
            return false;
        }
        if (tex.Data.size() * sizeof(u32) > MemoryCacheBudget)
        {
            // too large to keep: handed out once, decoded again next time
            out = std::move(tex.Data);
            outWidth = tex.Width;
            outHeight = tex.Height;
            return true;
        }
        Store(cacheKey, std::move(tex));
        cached = MemoryCache.find(cacheKey);
    }

    LRU.splice(LRU.begin(), LRU, cached->second.second);
    const CachedTexture& tex = cached->second.first;
    out = tex.Data;
    outWidth = tex.Width;
    outHeight = tex.Height;
    return true;
}

// reads and checks a replacement file; thread-safe (also run by the worker)
bool TextureReplacement::Decode(const std::string& path, u32 width, u32 height, bool binaryAlpha, u32 maxSize, CachedTexture& out)
{
    int w, h, comp;
    u8* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels)
    {
        Log(LogLevel::Warn, "TextureReplacement: cannot load %s: %s\n", path.c_str(), stbi_failure_reason());
        return false;
    }

    // Only power of two scales with the same factor on both axes are allowed,
    // the texture cache groups textures of the same size in array textures.
    u32 scale = (u32)w / width;
    bool valid = w > 0 && h > 0
        && (u32)w == width * scale && (u32)h == height * scale
        && scale >= 1 && scale <= 16 && (scale & (scale - 1)) == 0
        && (u32)w <= maxSize && (u32)h <= maxSize;
    if (!valid)
    {
        Log(LogLevel::Warn, "TextureReplacement: %s is %dx%d, expected %ux%u times 1, 2, 4, 8 or 16 (max %u)\n",
            path.c_str(), w, h, width, height, maxSize);
        stbi_image_free(pixels);
        return false;
    }

    out.Data.resize((size_t)w * h);
    for (size_t i = 0; i < out.Data.size(); i++)
    {
        const u8* p = &pixels[i * 4];
        u32 a = p[3];
        if (binaryAlpha)
            a = a >= 128 ? 255 : 0;
        out.Data[i] = a ? (p[0] | (p[1] << 8) | (p[2] << 16) | (a << 24)) : 0;
    }
    stbi_image_free(pixels);
    out.Width = w;
    out.Height = h;
    return true;
}

// into the memory cache, oldest entries evicted to stay within the budget;
// a texture larger than the whole budget is not kept
void TextureReplacement::Store(const std::string& cacheKey, CachedTexture&& tex)
{
    size_t bytes = tex.Data.size() * sizeof(u32);
    if (bytes > MemoryCacheBudget)
        return;
    while (MemoryCacheSize + bytes > MemoryCacheBudget && !LRU.empty())
    {
        auto victim = MemoryCache.find(LRU.back());
        MemoryCacheSize -= victim->second.first.Data.size() * sizeof(u32);
        MemoryCache.erase(victim);
        LRU.pop_back();
    }
    LRU.push_front(cacheKey);
    MemoryCache.emplace(cacheKey, std::make_pair(std::move(tex), LRU.begin()));
    MemoryCacheSize += bytes;
}

void TextureReplacement::Dump(u64 hash, const u32* data, u32 width, u32 height)
{
    if (!Config.Dump)
        return;

    std::string name = TextureName(hash, width, height);
    if (!Dumped.insert(name).second)
        return;

    std::vector<u8> rgba((size_t)width * height * 4);
    for (size_t i = 0; i < (size_t)width * height; i++)
        RGB6A5ToRGBA8(data[i], &rgba[i * 4]);

    std::string path = (fs::path(GameDir) / "dump" / (name + ".png")).string();
    if (!stbi_write_png(path.c_str(), width, height, 4, rgba.data(), width * 4))
        Log(LogLevel::Warn, "TextureReplacement: cannot write %s\n", path.c_str());
}

}
