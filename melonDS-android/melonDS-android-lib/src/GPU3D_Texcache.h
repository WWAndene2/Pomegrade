#ifndef GPU3D_TEXCACHE
#define GPU3D_TEXCACHE

#include "types.h"
#include "GPU.h"
#include "GPU3D_TextureReplacement.h"

#include <assert.h>
#include <unordered_map>
#include <vector>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

namespace melonDS
{

inline u32 TextureWidth(u32 texparam)
{
    return 8 << ((texparam >> 20) & 0x7);
}

inline u32 TextureHeight(u32 texparam)
{
    return 8 << ((texparam >> 23) & 0x7);
}

enum
{
    outputFmt_RGB6A5,
    outputFmt_RGBA8,
    outputFmt_BGRA8
};

template <int outputFmt>
void ConvertBitmapTexture(u32 width, u32 height, u32* output, u32 addr, GPU& gpu);
template <int outputFmt>
void ConvertCompressedTexture(u32 width, u32 height, u32* output, u32 addr, u32 addrAux, u32 palAddr, GPU& gpu);
template <int outputFmt, int X, int Y>
void ConvertAXIYTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, GPU& gpu);
template <int outputFmt, int colorBits>
void ConvertNColorsTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, bool color0Transparent, GPU& gpu);

// Where a texture's data lives in texture VRAM and texture palette VRAM
struct TexSource
{
    u32 TextureRAMStart[2], TextureRAMSize[2];
    u32 TexPalStart, TexPalSize;
};

// Cache key identifying a texture: its parameters without the sampling and
// texcoord generation bits, plus the palette base for paletted formats.
inline u64 TexcacheKey(u32 texParam, u32 palBase)
{
    texParam &= ~0xC00F0000;

    u32 fmt = (texParam >> 26) & 0x7;
    u64 key = texParam;
    if (fmt != 7)
    {
        key |= (u64)palBase << 32;
        if (fmt == 5)
            key &= ~((u64)1 << 29);
    }
    return key;
}

// Decodes a texture to RGB6A5 into `output` (TextureWidth*TextureHeight texels)
// and fills in where its data comes from.
void DecodeTexture(GPU& gpu, u32 texParam, u32 palBase, u32* output, TexSource& source);

u64 TexcacheMaskedHash(u8* vram, u32 vramSize, u32 addr, u32 size);

// Whether a texture range was modified, given the VRAM dirty bits
bool TexcacheCheckInvalid(u32 start, u32 size, u64 oldHash, u64* dirty, u8* vram, u32 vramSize);

template <typename TexLoaderT, typename TexHandleT>
class Texcache
{
public:
    Texcache(const TexLoaderT& texloader)
        : TexLoader(texloader) // probably better if this would be a move constructor???
    {}

    u64 MaskedHash(u8* vram, u32 vramSize, u32 addr, u32 size)
    {
        return TexcacheMaskedHash(vram, vramSize, addr, size);
    }

    bool CheckInvalid(u32 start, u32 size, u64 oldHash, u64* dirty, u8* vram, u32 vramSize)
    {
        return TexcacheCheckInvalid(start, size, oldHash, dirty, vram, vramSize);
    }

    bool Update(GPU& gpu)
    {
        // texture replacement settings changed: every texture has to be looked up again
        bool replacementChanged = false;
        Replacement.Active();
        if (Replacement.Generation() != ReplacementGeneration)
        {
            ReplacementGeneration = Replacement.Generation();
            for (auto& it : Cache)
                FreeTextures[it.second.WidthLog2][it.second.HeightLog2].push_back(it.second.Texture);
            replacementChanged = !Cache.empty();
            Cache.clear();
        }
        if (Replacement.Loaded() != ReplacementLoaded)
        {
            // replacements loaded in the background since: the textures that
            // were waiting for one are looked up again
            ReplacementLoaded = Replacement.Loaded();
            for (auto it = Cache.begin(); it != Cache.end();)
            {
                if (!it->second.ReplacementPending)
                {
                    it++;
                    continue;
                }
                FreeTextures[it->second.WidthLog2][it->second.HeightLog2].push_back(it->second.Texture);
                it = Cache.erase(it);
                replacementChanged = true;
            }
        }

        auto textureDirty = gpu.VRAMDirty_Texture.DeriveState(gpu.VRAMMap_Texture, gpu);
        auto texPalDirty = gpu.VRAMDirty_TexPal.DeriveState(gpu.VRAMMap_TexPal, gpu);

        bool textureChanged = gpu.MakeVRAMFlat_TextureCoherent(textureDirty);
        bool texPalChanged = gpu.MakeVRAMFlat_TexPalCoherent(texPalDirty);

        if (textureChanged || texPalChanged)
        {
            //printf("check invalidation %d\n", TexCache.size());
            for (auto it = Cache.begin(); it != Cache.end();)
            {
                TexCacheEntry& entry = it->second;
                if (textureChanged)
                {
                    for (u32 i = 0; i < 2; i++)
                    {
                        if (CheckInvalid(entry.TextureRAMStart[i], entry.TextureRAMSize[i],
                                entry.TextureHash[i],
                                textureDirty.Data,
                                gpu.VRAMFlat_Texture, sizeof(gpu.VRAMFlat_Texture)))
                            goto invalidate;
                    }
                }

                if (texPalChanged && entry.TexPalSize > 0)
                {
                    if (CheckInvalid(entry.TexPalStart, entry.TexPalSize,
                            entry.TexPalHash,
                            texPalDirty.Data,
                            gpu.VRAMFlat_TexPal, sizeof(gpu.VRAMFlat_TexPal)))
                        goto invalidate;
                }

                it++;
                continue;
            invalidate:
                FreeTextures[entry.WidthLog2][entry.HeightLog2].push_back(entry.Texture);

                //printf("invalidating texture %d\n", entry.ImageDescriptor);

                it = Cache.erase(it);
            }

            return true;
        }

        return replacementChanged;
    }

    void GetTexture(GPU& gpu, u32 texParam, u32 palBase, TexHandleT& textureHandle, u32& layer, u32*& helper)
    {
        u32 fmt = (texParam >> 26) & 0x7;
        u64 key = TexcacheKey(texParam, palBase);

        assert(fmt != 0 && "no texture is not a texture format!");

        auto it = Cache.find(key);

        if (it != Cache.end())
        {
            textureHandle = it->second.Texture.TextureID;
            layer = it->second.Texture.Layer;
            helper = &it->second.LastVariant;
            return;
        }

        // apparently a new texture
        u32 widthLog2 = (texParam >> 20) & 0x7;
        u32 heightLog2 = (texParam >> 23) & 0x7;
        u32 width = 8 << widthLog2;
        u32 height = 8 << heightLog2;

        TexCacheEntry entry = {0};
        TexSource source;
        DecodeTexture(gpu, texParam, palBase, DecodingBuffer, source);
        for (int i = 0; i < 2; i++)
        {
            entry.TextureRAMStart[i] = source.TextureRAMStart[i];
            entry.TextureRAMSize[i] = source.TextureRAMSize[i];
        }
        entry.TexPalStart = source.TexPalStart;
        entry.TexPalSize = source.TexPalSize;

        // HD texture replacement / dumping, keyed by the decoded contents
        u32* uploadData = DecodingBuffer;
        if (Replacement.Active())
        {
            u64 contentHash = TextureReplacement::HashDecoded(DecodingBuffer, width, height);
            Replacement.Dump(contentHash, DecodingBuffer, width, height);

            // A3I5 and A5I3 are the only formats with translucent texels
            bool binaryAlpha = fmt != 1 && fmt != 6;
            u32 maxSize = std::min<u32>(TexLoader.MaxTextureSize(), 8u << (MaxSizeLog2 - 1));
            u32 hdWidth, hdHeight;
            if (Replacement.Lookup(contentHash, width, height, binaryAlpha, maxSize, HDBuffer, hdWidth, hdHeight, &entry.ReplacementPending))
            {
                TextureReplacement::ConvertToRGB6A5(HDBuffer);
                width = hdWidth;
                height = hdHeight;
                widthLog2 = Log2(width / 8);
                heightLog2 = Log2(height / 8);
                uploadData = HDBuffer.data();
            }
        }
        // the free list this entry's storage goes back to when invalidated
        entry.WidthLog2 = widthLog2;
        entry.HeightLog2 = heightLog2;

        for (int i = 0; i < 2; i++)
        {
            if (entry.TextureRAMSize[i])
                entry.TextureHash[i] = MaskedHash(gpu.VRAMFlat_Texture, sizeof(gpu.VRAMFlat_Texture),
                    entry.TextureRAMStart[i], entry.TextureRAMSize[i]);
        }
        if (entry.TexPalSize)
            entry.TexPalHash = MaskedHash(gpu.VRAMFlat_TexPal, sizeof(gpu.VRAMFlat_TexPal),
                entry.TexPalStart, entry.TexPalSize);

        auto& texArrays = TexArrays[widthLog2][heightLog2];
        auto& freeTextures = FreeTextures[widthLog2][heightLog2];

        if (freeTextures.size() == 0)
        {
            texArrays.resize(texArrays.size()+1);
            TexHandleT& array = texArrays[texArrays.size()-1];

            u32 layers = std::max<u32>(std::min<u32>((8*1024*1024) / (width*height*4), 64), 1);

            // allocate new array texture
            //printf("allocating new layer set for %d %d %d %d\n", width, height, texArrays.size()-1, array.ImageDescriptor);
            array = TexLoader.GenerateTexture(width, height, layers);

            for (u32 i = 0; i < layers; i++)
            {
                freeTextures.push_back(TexArrayEntry{array, i});
            }
        }

        TexArrayEntry storagePlace = freeTextures[freeTextures.size()-1];
        freeTextures.pop_back();

        entry.Texture = storagePlace;

        TexLoader.UploadTexture(storagePlace.TextureID, width, height, storagePlace.Layer, uploadData);
        //printf("using storage place %d %d | %d %d (%d)\n", width, height, storagePlace.TexArrayIdx, storagePlace.LayerIdx, array.ImageDescriptor);

        textureHandle = storagePlace.TextureID;
        layer = storagePlace.Layer;
        helper = &Cache.emplace(std::make_pair(key, entry)).first->second.LastVariant;
    }

    void Reset()
    {
        for (u32 i = 0; i < MaxSizeLog2; i++)
        {
            for (u32 j = 0; j < MaxSizeLog2; j++)
            {
                for (u32 k = 0; k < TexArrays[i][j].size(); k++)
                    TexLoader.DeleteTexture(TexArrays[i][j][k]);
                TexArrays[i][j].clear();
                FreeTextures[i][j].clear();
            }
        }
        Cache.clear();
    }
private:
    // native textures go up to 1024 (8 << 7), replacements up to 4096 (8 << 9)
    static constexpr u32 MaxSizeLog2 = 10;

    static u32 Log2(u32 v)
    {
        u32 r = 0;
        while (v >>= 1) r++;
        return r;
    }

    struct TexArrayEntry
    {
        TexHandleT TextureID;
        u32 Layer;
    };

    struct TexCacheEntry
    {
        u32 LastVariant; // very cheap way to make variant lookup faster

        u32 TextureRAMStart[2], TextureRAMSize[2];
        u32 TexPalStart, TexPalSize;
        u8 WidthLog2, HeightLog2;
        TexArrayEntry Texture;

        u64 TextureHash[2];
        u64 TexPalHash;
        bool ReplacementPending; // its replacement is being loaded in the background
    };
    std::unordered_map<u64, TexCacheEntry> Cache;

    TexLoaderT TexLoader;

    std::vector<TexArrayEntry> FreeTextures[MaxSizeLog2][MaxSizeLog2];
    std::vector<TexHandleT> TexArrays[MaxSizeLog2][MaxSizeLog2];

    u32 DecodingBuffer[1024*1024];

    TextureReplacement Replacement;
    u32 ReplacementGeneration = 0;
    u32 ReplacementLoaded = 0;
    std::vector<u32> HDBuffer;
};

}

#endif