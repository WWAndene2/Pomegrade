#ifndef GPU_SCENECOLOUR_H
#define GPU_SCENECOLOUR_H

// OLED deep blacks (Pomegrade): from a small copy of each frame, before any
// adjustment, derive a threshold under which near-black pixels become true
// black (pixels off on OLED screens), following the scene's black level.
// The threshold moves smoothly from frame to frame so the image doesn't pump.

#include "types.h"

namespace melonDS
{

struct SceneStats
{
    float Black = 0;      // 2nd percentile of luma, 0..1
};

struct SceneColourParams
{
    float OledThreshold = 0; // luma under which pixels fade to true black, 0 = off
};

class SceneColour
{
public:
    // RGBA8 samples (r | g<<8 | b<<16 | a<<24)
    static SceneStats Measure(const u32* rgba, u32 count);

    // Target parameters for a scene.
    static SceneColourParams Target(const SceneStats& stats);

    // Moves current a step towards target (call once per frame).
    static void Step(SceneColourParams& current, const SceneColourParams& target);

    // The adjustment the compositor applies, on one RGB colour (0..1), for tests.
    static void Apply(const SceneColourParams& params, float* rgb);
};

}

#endif
