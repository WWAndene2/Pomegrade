#ifndef GPU3D_OPENGL_HDTEXTURES_H
#define GPU3D_OPENGL_HDTEXTURES_H

// HD texture replacement for the classic OpenGL renderer (Pomegrade).
//
// That renderer decodes textures in the fragment shader straight from texture
// VRAM, so there is no texture cache to swap textures in. Instead, textures
// that have a replacement are uploaded to an atlas (a 2D array texture, each
// layer split in cells of a single size) and polygons using them carry the
// location of their cell to the shader, which samples it instead of VRAM.

#include "GPU3D_Texcache.h"
#include "GPU3D_TextureReplacement.h"
#include "OpenGLSupport.h"

#include <unordered_map>
#include <vector>

namespace melonDS
{

class GLHDTextures
{
public:
    // Size of an atlas layer, also the largest replacement this path handles.
    static constexpr u32 LayerSize = 1024;
    // 4 MB per layer, so at most 128 MB of video memory
    static constexpr u32 MaxLayers = 32;

    GLHDTextures() = default;
    ~GLHDTextures();
    GLHDTextures(const GLHDTextures&) = delete;
    GLHDTextures& operator=(const GLHDTextures&) = delete;

    // Drops all textures (GL context must be current).
    void Reset();

    // Call once per frame before Lookup: picks up configuration changes and
    // drops textures whose VRAM contents changed. Returns whether texture
    // replacement or dumping is enabled.
    bool BeginFrame(GPU& gpu);

    // Returns the packed atlas location for a polygon's texture, 0 if it has
    // no replacement. Layout (fits in 26 bits):
    //   bits 0-6   cell x / 8
    //   bits 7-13  cell y / 8
    //   bits 14-21 atlas layer
    //   bits 22-24 log2 of the replacement's scale
    //   bit  25    set
    u32 Lookup(GPU& gpu, u32 texParam, u32 palBase);

    GLuint AtlasTexture() const { return Atlas; }

    // Native texture upscaling (Pomegrade): textures without a pack
    // replacement are magnified x2 or x4 with TextureUpscaler. 1 = off.
    void SetUpscaleFactor(int factor);

    // The atlas was rebuilt during the last Lookup calls: locations returned
    // before that are stale and every polygon has to be looked up again.
    bool ConsumeAtlasRebuilt()
    {
        bool rebuilt = AtlasRebuilt;
        AtlasRebuilt = false;
        return rebuilt;
    }

private:
    struct Entry
    {
        TexSource Source;
        u64 TextureHash[2];
        u64 TexPalHash;
        u32 Info;      // packed location, 0 = no replacement
        s32 SizeClass; // -1 = no replacement
        u32 Cell;
    };

    struct SizeClass
    {
        u32 Width, Height;
        std::vector<u32> FreeCells; // layer << 16 | cell index
    };

    bool AllocCell(u32 width, u32 height, s32& sizeClass, u32& cell, u32& x, u32& y, u32& layer);
    void FreeCell(s32 sizeClass, u32 cell);
    bool GrowAtlas();
    void ClearEntries();

    TextureReplacement Replacement;
    u32 ReplacementGeneration = 0;
    bool Enabled = false;
    int UpscaleFactor = 1;

    std::unordered_map<u64, Entry> Cache;
    std::vector<SizeClass> SizeClasses;
    std::vector<s32> LayerOwner; // size class owning each layer, -1 = free

    GLuint Atlas = 0;
    u32 NumLayers = 0;
    bool AtlasRebuilt = false;
    bool AtlasFullWarned = false;

    u32 DecodingBuffer[1024*1024];
    std::vector<u32> HDBuffer;
};

}

#endif
