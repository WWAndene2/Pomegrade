#ifndef GPU_SCENECOLOUR_H
#define GPU_SCENECOLOUR_H

// Scene-adaptive colour (Pomegrade): from a small copy of each frame, before
// any adjustment, derive
//   - adaptive colours: levels and saturation that stretch dull, washed-out
//     scenes and leave already contrasted ones alone;
//   - OLED deep blacks: a threshold under which near-black pixels become true
//     black (pixels off on OLED screens), following the scene's black level.
// Parameters move smoothly from frame to frame so the image doesn't pump.

#include "types.h"

namespace melonDS
{

struct SceneStats
{
    float Black = 0;      // 2nd percentile of luma, 0..1
    float White = 1;      // 98th percentile of luma
    float Saturation = 0; // mean (max - min) of the RGB channels
};

struct SceneColourParams
{
    float LevelBlack = 0;    // output = (input - LevelBlack) / (LevelWhite - LevelBlack)
    float LevelWhite = 1;
    float Saturation = 1;    // 1 = unchanged
    float OledThreshold = 0; // luma under which pixels fade to true black, 0 = off
};

class SceneColour
{
public:
    // RGBA8 samples (r | g<<8 | b<<16 | a<<24)
    static SceneStats Measure(const u32* rgba, u32 count);

    // Target parameters for a scene.
    static SceneColourParams Target(const SceneStats& stats, bool adaptive, bool oled);

    // Moves current a step towards target (call once per frame).
    static void Step(SceneColourParams& current, const SceneColourParams& target);

    // The adjustment the compositor applies, on one RGB colour (0..1), for tests.
    static void Apply(const SceneColourParams& params, float* rgb);
};

}

#endif
