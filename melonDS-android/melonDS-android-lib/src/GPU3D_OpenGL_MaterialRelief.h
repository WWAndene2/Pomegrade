#ifndef GPU3D_OPENGL_MATERIALRELIEF_H
#define GPU3D_OPENGL_MATERIALRELIEF_H

// Relief by material (Pomegrade, DS_ENGINE_REMAKE.md 14.1 and 7.3): relief
// textures read each texel's brightness as height, which suits stone and
// wood but bumps painted skies, flat water and painted character shading.
// Each texture the classic OpenGL renderer draws is classified once from its
// texels (MaterialClassifier, texture statistics only: the renderer has no
// render-state evidence) and gets a relief depth by class, in eighths of the
// relief setting, carried to the shader in the polygon attributes' bits 0-3.
// Foliage also sets bit 12 (VolumetricGrass): the shader draws it as a slab
// of grass blades instead of parallax.

#include "GPU3D_MaterialClassifier.h"

#include <unordered_map>

namespace melonDS
{
class GPU;

class GLMaterialRelief
{
public:
    // once per frame before Scale: texture VRAM or palettes changed since the
    // last frame (GLHDTextures::TexturesChanged), so classes may be stale
    void BeginFrame(bool texturesChanged);
    static constexpr u32 VolumetricGrass = 1 << 12;
    // relief depth of a polygon's texture, 0-15 eighths, | VolumetricGrass
    u32 Scale(GPU& gpu, u32 texParam, u32 palette);
    void Reset() { Classifier.Clear(); Scales.clear(); }

    static u32 ScaleOf(MaterialClass c);

private:
    MaterialClassifier Classifier;
    std::unordered_map<u64, u32> Scales; // by TexcacheKey
};

}

#endif // GPU3D_OPENGL_MATERIALRELIEF_H
