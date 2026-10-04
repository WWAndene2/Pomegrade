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
    case MaterialClass::Foliage: return 6 | VolumetricGrass;
    case MaterialClass::Lava: return 4;
    case MaterialClass::Character: return 4 | Fabric; // cloth folds; skin is left smooth in the shader
    case MaterialClass::Sky: return 0;
    case MaterialClass::Water: return 0;      // a flat surface; its waves are not in the texels
    default: return 4;                        // unknown: atlases of several materials, half depth
    }
}

void GLMaterialRelief::BeginFrame(bool texturesChanged, int sceneryId)
{
    // statistics dropped at once: textures change mostly on scene loads.
    // Classes are voted again each frame, as the polygon IDs that make a
    // character change with what the frame draws (cheap: statistics are kept)
    if (texturesChanged) Classifier.Clear();
    Scales.clear();
    SceneryId = sceneryId;
}

u32 GLMaterialRelief::Scale(GPU& gpu, u32 texParam, u32 palette, u64 polygonIds)
{
    const u64 key = TexcacheKey(texParam, palette);
    auto found = Scales.find(key);
    if (found != Scales.end()) return found->second;

    const MaterialClassifier::TextureStats& s = Classifier.Statistics(gpu, texParam, palette);
    TextureEvidence e;
    e.Polygons = 1;
    e.PolygonIds = polygonIds;
    e.SceneryId = SceneryId;
    const u32 scale = ScaleOf(MaterialClassifier::Vote(s, texParam, e).Class);
    Scales.emplace(key, scale);
    return scale;
}

}
