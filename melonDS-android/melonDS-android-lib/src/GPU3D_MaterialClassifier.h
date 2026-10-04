#ifndef GPU3D_MATERIALCLASSIFIER_H
#define GPU3D_MATERIALCLASSIFIER_H

// Material classifier (Pomegrade, DS_ENGINE_REMAKE.md 7.1 and 5.12 step 7):
// what a texture is (water, foliage, wood, stone...) from two of the
// classifier's layers, with no learning:
// - render state: translucent polygons, a scrolling texture matrix, a cut-out
//   alpha (1-bit or a transparent colour 0), a lit polygon's emission;
// - texture statistics, computed once per texture and palette: hue and
//   saturation, brightness, high-frequency energy, grain direction.
// Each cue votes for classes with the doc's weights (render state 0.6,
// statistics 0.3); the best class wins with a confidence, below 0.35 it is
// Unknown. The manifest and file-name layers are not done yet.

#include "types.h"

#include <map>
#include <string>

namespace melonDS
{
class GPU;

enum class MaterialClass : int { Unknown, Water, Lava, Foliage, Wood, Stone, Sky, Count };

// what the game's polygons say about a texture, over a frame
struct TextureEvidence
{
    u32 Polygons = 0;
    u32 Translucent = 0;  // polygons with alpha 1-30
    bool Scrolling = false;
    // material registers of its lit polygons (DIF_AMB, SPE_EMI)
    u32 Lit = 0;
    u8 Specular = 0, Emission = 0; // largest channel, 0-31
    bool Shininess = false;
};

struct MaterialResult
{
    MaterialClass Class = MaterialClass::Unknown;
    float Confidence = 0;
    std::string Cues;     // the cues that voted, for the report
};

class MaterialClassifier
{
public:
    // texParam/palette as the polygon has them (TexParam, TexPalette)
    MaterialResult Classify(GPU& gpu, u32 texParam, u32 palette, const TextureEvidence& evidence);
    void Clear() noexcept { Stats.clear(); }

    [[nodiscard]] static const char* Name(MaterialClass c) noexcept;

    static constexpr float MinConfidence = 0.35f;

    // a texture's statistics (exposed for the report and tests)
    struct TextureStats
    {
        float Hue = 0;        // degrees, of the mean colour of opaque texels
        float Saturation = 0; // 0-1
        float Brightness = 0; // 0-1
        float Detail = 0;     // mean neighbour difference, 0-1 (busy: grass, stone)
        float Grain = 0;      // 0: isotropic, 1: all along one axis (wood)
        float Transparent = 0; // share of transparent texels
        float Variety = 0;    // mean distance of texels from the mean colour, 0-1 (an atlas of many colours: high)
    };
    const TextureStats& Statistics(GPU& gpu, u32 texParam, u32 palette);

private:
    std::map<u64, TextureStats> Stats;
};

}

#endif // GPU3D_MATERIALCLASSIFIER_H
