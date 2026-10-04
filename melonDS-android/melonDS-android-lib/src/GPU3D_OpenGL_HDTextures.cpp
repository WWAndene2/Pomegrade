#include "GPU3D_OpenGL_HDTextures.h"
#include "GPU3D_TextureUpscaler.h"
#include "Platform.h"

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

GLHDTextures::~GLHDTextures()
{
    if (Atlas)
        glDeleteTextures(1, &Atlas);
}

void GLHDTextures::ClearEntries()
{
    Cache.clear();
    SizeClasses.clear();
    LayerOwner.assign(NumLayers, -1);
}

void GLHDTextures::Reset()
{
    ClearEntries();
    if (Atlas)
        glDeleteTextures(1, &Atlas);
    Atlas = 0;
    NumLayers = 0;
    LayerOwner.clear();
    AtlasRebuilt = false;
    AtlasFullWarned = false;
}

void GLHDTextures::SetUpscaleFactor(int factor)
{
    // a power of two, up to x16
    int power = 1;
    while (power < 16 && power * 2 <= factor) power *= 2;
    factor = power;
    if (factor == UpscaleFactor)
        return;
    UpscaleFactor = factor;
    ClearEntries();
}

bool GLHDTextures::BeginFrame(GPU& gpu)
{
    Enabled = Replacement.Active() || UpscaleFactor > 1;
    if (Replacement.Generation() != ReplacementGeneration)
    {
        // settings, game or replacement files changed
        ReplacementGeneration = Replacement.Generation();
        ClearEntries();
    }
    Changed = false;
    if (!Enabled && !KeepCoherent)
    {
        WasCoherent = false;
        return false;
    }

    auto textureDirty = gpu.VRAMDirty_Texture.DeriveState(gpu.VRAMMap_Texture, gpu);
    auto texPalDirty = gpu.VRAMDirty_TexPal.DeriveState(gpu.VRAMMap_TexPal, gpu);

    bool textureChanged = gpu.MakeVRAMFlat_TextureCoherent(textureDirty);
    bool texPalChanged = gpu.MakeVRAMFlat_TexPalCoherent(texPalDirty);
    // the first coherent frame after a pause: anything may have changed
    Changed = textureChanged || texPalChanged || !WasCoherent;
    WasCoherent = true;
    if (!Enabled)
    {
        // the changes are consumed here: textures kept from before replacement
        // was turned off would be stale when it is turned back on
        if (textureChanged || texPalChanged) ClearEntries();
        return false;
    }

    if (textureChanged || texPalChanged)
    {
        for (auto it = Cache.begin(); it != Cache.end();)
        {
            Entry& entry = it->second;
            bool invalid = false;

            if (textureChanged)
            {
                for (u32 i = 0; i < 2 && !invalid; i++)
                {
                    if (entry.Source.TextureRAMSize[i])
                        invalid = TexcacheCheckInvalid(entry.Source.TextureRAMStart[i], entry.Source.TextureRAMSize[i],
                            entry.TextureHash[i], textureDirty.Data,
                            gpu.VRAMFlat_Texture, sizeof(gpu.VRAMFlat_Texture));
                }
            }
            if (!invalid && texPalChanged && entry.Source.TexPalSize > 0)
            {
                invalid = TexcacheCheckInvalid(entry.Source.TexPalStart, entry.Source.TexPalSize,
                    entry.TexPalHash, texPalDirty.Data,
                    gpu.VRAMFlat_TexPal, sizeof(gpu.VRAMFlat_TexPal));
            }

            if (invalid)
            {
                if (entry.SizeClass >= 0)
                    FreeCell(entry.SizeClass, entry.Cell);
                it = Cache.erase(it);
            }
            else
                it++;
        }
    }

    return true;
}

bool GLHDTextures::GrowAtlas()
{
    if (NumLayers >= MaxLayers)
        return false;

    GLint maxLayers = 0, maxSize = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (maxSize < (GLint)LayerSize)
        return false;

    u32 newLayers = NumLayers ? NumLayers * 2 : 2;
    if (newLayers > MaxLayers) newLayers = MaxLayers;
    if (maxLayers > 0 && newLayers > (u32)maxLayers) newLayers = maxLayers;
    if (newLayers <= NumLayers)
        return false;

    // Immutable storage can't be resized: start over with a bigger atlas.
    // Replacements come back from TextureReplacement's memory cache.
    bool hadAtlas = Atlas != 0;
    if (Atlas)
        glDeleteTextures(1, &Atlas);

    glActiveTexture(GL_TEXTURE2);
    glGenTextures(1, &Atlas);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Atlas);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, LayerSize, LayerSize, newLayers);

    NumLayers = newLayers;
    ClearEntries();
    if (hadAtlas)
        AtlasRebuilt = true;

    Log(LogLevel::Info, "HD textures: atlas now has %u layers\n", NumLayers);
    return true;
}

bool GLHDTextures::AllocCell(u32 width, u32 height, s32& sizeClass, u32& cell, u32& x, u32& y, u32& layer)
{
    sizeClass = -1;
    for (u32 i = 0; i < SizeClasses.size(); i++)
    {
        if (SizeClasses[i].Width == width && SizeClasses[i].Height == height)
        {
            sizeClass = i;
            break;
        }
    }
    if (sizeClass < 0)
    {
        sizeClass = SizeClasses.size();
        SizeClasses.push_back(SizeClass{width, height, {}});
    }

    SizeClass& sc = SizeClasses[sizeClass];
    if (sc.FreeCells.empty())
    {
        // hand a whole free layer to this size class
        s32 freeLayer = -1;
        for (u32 i = 0; i < LayerOwner.size(); i++)
        {
            if (LayerOwner[i] < 0)
            {
                freeLayer = i;
                break;
            }
        }
        if (freeLayer < 0)
            return false;

        LayerOwner[freeLayer] = sizeClass;
        u32 numCells = (LayerSize / width) * (LayerSize / height);
        // reversed so cells get used in order
        for (u32 i = numCells; i-- > 0;)
            sc.FreeCells.push_back(((u32)freeLayer << 16) | i);
    }

    cell = sc.FreeCells.back();
    sc.FreeCells.pop_back();

    u32 cellsPerRow = LayerSize / width;
    u32 index = cell & 0xFFFF;
    layer = cell >> 16;
    x = (index % cellsPerRow) * width;
    y = (index / cellsPerRow) * height;
    return true;
}

void GLHDTextures::FreeCell(s32 sizeClass, u32 cell)
{
    SizeClasses[sizeClass].FreeCells.push_back(cell);
}

u32 GLHDTextures::Lookup(GPU& gpu, u32 texParam, u32 palBase)
{
    u64 key = TexcacheKey(texParam, palBase);
    auto it = Cache.find(key);
    if (it != Cache.end())
        return it->second.Info;

    u32 fmt = (texParam >> 26) & 0x7;
    u32 width = TextureWidth(texParam);
    u32 height = TextureHeight(texParam);

    Entry entry = {};
    entry.SizeClass = -1;
    DecodeTexture(gpu, texParam, palBase, DecodingBuffer, entry.Source);

    for (u32 i = 0; i < 2; i++)
    {
        if (entry.Source.TextureRAMSize[i])
            entry.TextureHash[i] = TexcacheMaskedHash(gpu.VRAMFlat_Texture, sizeof(gpu.VRAMFlat_Texture),
                entry.Source.TextureRAMStart[i], entry.Source.TextureRAMSize[i]);
    }
    if (entry.Source.TexPalSize)
        entry.TexPalHash = TexcacheMaskedHash(gpu.VRAMFlat_TexPal, sizeof(gpu.VRAMFlat_TexPal),
            entry.Source.TexPalStart, entry.Source.TexPalSize);

    u64 contentHash = TextureReplacement::HashDecoded(DecodingBuffer, width, height);
    Replacement.Dump(contentHash, DecodingBuffer, width, height);

    // A3I5 and A5I3 are the only formats with translucent texels
    bool binaryAlpha = fmt != 1 && fmt != 6;
    u32 hdWidth, hdHeight;
    bool haveHD = Replacement.Lookup(contentHash, width, height, binaryAlpha, LayerSize, HDBuffer, hdWidth, hdHeight);
    // no pack replacement: native upscaling, if enabled, at the largest factor
    // up to the setting whose result fits an atlas layer (x16 of a 128x128
    // texture would be 2048x2048)
    int upscaled = 1;
    if (!haveHD)
    {
        upscaled = UpscaleFactor;
        while (upscaled > 1 && (width * upscaled > LayerSize || height * upscaled > LayerSize))
            upscaled /= 2;
        if (upscaled > 1)
        {
            TextureUpscaler::Upscale(DecodingBuffer, width, height, upscaled, binaryAlpha,
                                     TextureUpscaler::EdgeFromTexParam(texParam, 0), TextureUpscaler::EdgeFromTexParam(texParam, 1),
                                     HDBuffer);
            hdWidth = width * upscaled;
            hdHeight = height * upscaled;
            haveHD = true;
        }
    }
    if (haveHD)
    {
        u32 x, y, layer, scaleLog2 = 0;
        while ((width << scaleLog2) < hdWidth) scaleLog2++;

        bool allocated = AllocCell(hdWidth, hdHeight, entry.SizeClass, entry.Cell, x, y, layer);
        if (!allocated && GrowAtlas())
        {
            // everything was dropped, the entry has to be redone from scratch
            return Lookup(gpu, texParam, palBase);
        }
        // atlas at its size limit: an upscaled texture steps down a factor
        // (x16 -> x8 -> ... -> x2) rather than staying at native resolution
        while (!allocated && upscaled > 2)
        {
            upscaled /= 2;
            TextureUpscaler::Upscale(DecodingBuffer, width, height, upscaled, binaryAlpha,
                                     TextureUpscaler::EdgeFromTexParam(texParam, 0), TextureUpscaler::EdgeFromTexParam(texParam, 1),
                                     HDBuffer);
            hdWidth = width * upscaled;
            hdHeight = height * upscaled;
            scaleLog2 = 0;
            while ((width << scaleLog2) < hdWidth) scaleLog2++;
            allocated = AllocCell(hdWidth, hdHeight, entry.SizeClass, entry.Cell, x, y, layer);
        }

        if (allocated)
        {
            // HDBuffer is RGBA8, uploaded at full precision
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D_ARRAY, Atlas);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, layer, hdWidth, hdHeight, 1,
                GL_RGBA, GL_UNSIGNED_BYTE, HDBuffer.data());

            entry.Info = (x >> 3) | ((y >> 3) << 7) | (layer << 14) | (scaleLog2 << 22) | (1 << 25);
        }
        else
        {
            entry.SizeClass = -1;
            if (!AtlasFullWarned)
            {
                Log(LogLevel::Warn, "HD textures: atlas full, some textures stay at native resolution\n");
                AtlasFullWarned = true;
            }
        }
    }

    Cache.emplace(key, entry);
    return entry.Info;
}

}
