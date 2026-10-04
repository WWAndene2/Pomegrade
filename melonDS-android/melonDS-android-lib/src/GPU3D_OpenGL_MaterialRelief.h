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
// of grass blades instead of parallax. Characters (a texture of many colours
// drawn with a polygon ID apart from the scenery's, the frame's IDs being the
// renderer's evidence) set bit 13 (Fabric): skin stays smooth, the rest is
// drawn as cloth, folds shaded from its painted light and a fine weave.
// Stone and wood are redrawn as procedural surfaces fitted to the texture
// (section 16.1): no texel grid, detail at any resolution.

#include "GPU3D_MaterialClassifier.h"

#include <unordered_map>

namespace melonDS
{
class GPU;

class GLMaterialRelief
{
public:
    // once per frame before Scale: texture VRAM or palettes changed since the
    // last frame (GLHDTextures::TexturesChanged), so statistics may be stale;
    // sceneryId: the frame's scenery polygon ID (MaterialClassifier::SceneryId)
    void BeginFrame(bool texturesChanged, int sceneryId);
    static constexpr u32 VolumetricGrass = 1 << 12, Fabric = 1 << 13;
    // procedural surface (DS_ENGINE_REMAKE.md 16.1), bits 16-18 of Scale's
    // result, for the shader in the texture address attribute's bits 16-18:
    // the texture's smoothed colours with detail generated at any resolution
    // wood streaks only where the texture has a clear grain (Grain 0.3 or
    // more, as the classifier's "grain" cue): otherwise its axis is noise
    // Smooth alone: the colour without its texel grid and no detail (sky, water)
    static constexpr u32 ProceduralShift = 16, ProceduralStone = 1, ProceduralWood = 2, ProceduralWoodNoGrain = 3, GrainAlongT = 4, Smooth = 8;
    // relief depth of a polygon's texture, 0-15 eighths, | VolumetricGrass or
    // Fabric; polygonIds: bit n set when this frame drew it with polygon ID n
    u32 Scale(GPU& gpu, u32 texParam, u32 palette, u64 polygonIds);
    void Reset() { Classifier.Clear(); Scales.clear(); }

    static u32 ScaleOf(MaterialClass c);

private:
    MaterialClassifier Classifier; // its statistics persist until textures change
    std::unordered_map<u64, u32> Scales; // this frame's, by TexcacheKey
    int SceneryId = -1;
};

}

#endif // GPU3D_OPENGL_MATERIALRELIEF_H
