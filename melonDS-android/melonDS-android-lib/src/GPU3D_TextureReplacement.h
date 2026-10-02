#ifndef GPU3D_TEXTUREREPLACEMENT_H
#define GPU3D_TEXTUREREPLACEMENT_H

// HD texture replacement and texture dumping for the 3D renderer (Pomegrade).
//
// Textures are identified by a hash of their *decoded* contents (so the same
// texture is recognised regardless of where the game puts it in VRAM).
// Files live in <root>/<GAMECODE>/:
//   - dump/<name>.png     textures dumped from the game, at native resolution
//   - <name>.png          replacements (anywhere below <root>/<GAMECODE>/
//                         except dump/), at 1x, 2x, 4x, 8x or 16x resolution
// <name> is "tex_<width>x<height>_<hash, 16 hex digits>".

#include "types.h"

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace melonDS
{

struct TextureReplacementConfig
{
    std::string RootDir;   // empty = feature disabled
    bool Replace = false;  // load HD replacements
    bool Dump = false;     // write decoded textures to dump/
};

class TextureReplacement
{
public:
    // Thread-safe, can be called from the frontend at any time.
    // Changes are picked up by the renderer on its next texture lookup.
    static void SetConfig(const TextureReplacementConfig& config);
    static void SetGameCode(const std::string& gameCode);

    // Called by the texture cache (render thread).
    bool Active();

    // Changes whenever the configuration, game or set of replacement files
    // changes: textures cached before that have to be looked up again.
    u32 Generation() const { return SeenGeneration; }

    // Content hash of a decoded texture (RGB6A5 layout, one u32 per texel).
    static u64 HashDecoded(const u32* data, u32 width, u32 height);

    // Looks up a replacement. On success, `out` holds the texture as RGBA8
    // (one u32 per texel, bytes in R, G, B, A order) and outWidth/outHeight
    // are the native size times the scale.
    // binaryAlpha: the original format has no translucency, so alpha is
    // forced to 0 or 255 (translucent texels would change how the polygon is
    // blended).
    bool Lookup(u64 hash, u32 width, u32 height, bool binaryAlpha, u32 maxSize,
                std::vector<u32>& out, u32& outWidth, u32& outHeight);

    // Converts Lookup's output in place to the texture cache's RGB6A5 layout.
    static void ConvertToRGB6A5(std::vector<u32>& data);

    void Dump(u64 hash, const u32* data, u32 width, u32 height);

    static std::string TextureName(u64 hash, u32 width, u32 height);

    // Decoded replacements kept in RAM so that textures the game re-uploads
    // don't have to be decoded from disk again.
    static constexpr size_t MemoryCacheBudget = 128 * 1024 * 1024;

private:
    void Refresh();
    void IndexDirectory();

    struct CachedTexture
    {
        std::vector<u32> Data;
        u32 Width, Height;
    };

    u32 SeenGeneration = 0;
    TextureReplacementConfig Config;
    std::string GameDir;

    std::unordered_map<std::string, std::string> Index; // name -> file path
    std::unordered_set<std::string> Dumped;
    std::unordered_set<std::string> Rejected; // replacements with invalid size/format

    std::list<std::string> LRU; // most recently used first
    std::unordered_map<std::string, std::pair<CachedTexture, std::list<std::string>::iterator>> MemoryCache;
    size_t MemoryCacheSize = 0;

    static std::mutex ConfigMutex;
    static TextureReplacementConfig SharedConfig;
    static std::string SharedGameCode;
    static u32 SharedGeneration;
};

}

#endif
