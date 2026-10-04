#include "GPU3D_OpenGL_MaterialRelief.h"

#include "GPU3D_Texcache.h"

namespace melonDS
{

u32 GLMaterialRelief::ScaleOf(MaterialClass c)
{
    switch (c)
    {
    case MaterialClass::Stone: return 8;      // the relief setting as is
    case MaterialClass::Wood: return 6;
    case MaterialClass::Foliage: return 6;
    case MaterialClass::Lava: return 4;
    case MaterialClass::Character: return 2;  // painted shading, not height
    case MaterialClass::Sky: return 0;
    case MaterialClass::Water: return 0;      // a flat surface; its waves are not in the texels
    default: return 4;                        // unknown: atlases of several materials, half depth
    }
}

void GLMaterialRelief::BeginFrame(bool texturesChanged)
{
    // all classes dropped at once: textures change mostly on scene loads
    if (texturesChanged) Reset();
}

u32 GLMaterialRelief::Scale(GPU& gpu, u32 texParam, u32 palette)
{
    const u64 key = TexcacheKey(texParam, palette);
    auto found = Scales.find(key);
    if (found != Scales.end()) return found->second;

    const MaterialClassifier::TextureStats& s = Classifier.Statistics(gpu, texParam, palette);
    const u32 scale = ScaleOf(MaterialClassifier::Vote(s, texParam, TextureEvidence{}).Class);
    Scales.emplace(key, scale);
    return scale;
}

}
