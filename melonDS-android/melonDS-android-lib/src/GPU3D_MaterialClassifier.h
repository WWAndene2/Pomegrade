#ifndef GPU3D_MATERIALCLASSIFIER_H
#define GPU3D_MATERIALCLASSIFIER_H

// Material classifier (Pomegrade, DS_ENGINE_REMAKE.md 7.1 and 5.12 step 7):
// what a texture is (water, foliage, wood, stone...) from two of the
// classifier's layers, with no learning:
// - render state: translucent polygons, a scrolling texture matrix, a cut-out
//   alpha (1-bit or a transparent colour 0), a lit polygon's emission, its
//   polygon IDs (games give each character its own, for its outline);
// - texture statistics, computed once per texture and palette: hue and
//   saturation, brightness, high-frequency energy, grain direction.
// Each cue votes for classes with the doc's weights (render state 0.6,
// statistics 0.3); the best class wins with a confidence, below 0.35 it is
// Unknown. Above all, a manual manifest (7.1 layer 1) names textures by
// content hash and always wins. The file-name layer is not done yet.

#include "types.h"

#include <map>
#include <string>
#include <vector>

namespace melonDS
{
class GPU;

enum class MaterialClass : int { Unknown, Water, Lava, Foliage, Wood, Stone, Sky, Character, Count };

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
    u64 PolygonIds = 0;  // bit n: polygon ID n
    // the frame's scenery ID (the one shared by the most textures), -1: none
    int SceneryId = -1;
};

struct MaterialResult
{
    MaterialClass Class = MaterialClass::Unknown;
    float Confidence = 0;
    std::string Cues;     // the cues that voted, for the report
};

// the manual manifest: class by texture content hash
using MaterialManifest = std::map<u32, MaterialClass>;

class MaterialClassifier
{
public:
    // texParam/palette as the polygon has them (TexParam, TexPalette)
    MaterialResult Classify(GPU& gpu, u32 texParam, u32 palette, const TextureEvidence& evidence);
    void Clear() noexcept { Stats.clear(); }

    // the manifest: one texture a line, "<hash> <class>" (hash in hex as the
    // report shows it, class as Name() writes it), # starts a comment.
    // Returns the lines it could not read, one a line
    static std::string ParseManifest(const std::string& text, MaterialManifest& out);
    void SetManifest(MaterialManifest manifest) { Manifest = std::move(manifest); }
    [[nodiscard]] size_t ManifestSize() const noexcept { return Manifest.size(); }

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
        u32 Hash = 0;         // XXH32 of the decoded texels: the same texture wherever it is in VRAM
        float Variety = 0;    // mean chroma distance of texels from the mean colour, 0-1 (an atlas of many colours: high)
    };
    const TextureStats& Statistics(GPU& gpu, u32 texParam, u32 palette);

    // palette-index segmentation (DS_ENGINE_REMAKE.md 5.3): artists paint
    // each material of a paletted texture with its own palette entries. The
    // entries the texture uses are grouped by colour (scaled to its brightest
    // channel: shading changes a material's lightness, not its colour), each
    // group a region
    struct Region
    {
        std::vector<u8> Indices;
        float Share = 0;      // of the opaque texels
        u16 Colour = 0;       // mean, BGR555
        const char* Label = ""; // "skin" when its colour is in skin's band
    };
    // empty for direct colour, compressed and translucent (A3I5, A5I3) formats
    std::vector<Region> Segment(GPU& gpu, u32 texParam, u32 palette) const;

    static constexpr float RegionDistance = 0.2f;  // colour distance merging two entries' groups

private:
    std::map<u64, TextureStats> Stats;
    MaterialManifest Manifest;
};

}

#endif // GPU3D_MATERIALCLASSIFIER_H
