#ifndef GPU3D_LIGHTRECOVERY_H
#define GPU3D_LIGHTRECOVERY_H

// Light recovery (Pomegrade, DS_ENGINE_REMAKE.md 5.12 step 11): the DS has
// four directional lights, set by LIGHT_VECTOR (direction, transformed by the
// vector matrix in effect) and LIGHT_COLOR. From one frame's command trace
// this gives every setting each light had, in order, and how often it was
// sent: the scene's lights to rebuild with real lighting.

#include "types.h"
#include "GPU3D_SkeletonRecovery.h" // GeometryCommand

#include <string>
#include <vector>

namespace melonDS
{

struct LightSetting
{
    int Light = 0;           // 0-3
    float Direction[3] = {}; // as sent (1.9 fixed point), before the vector matrix
    u16 Colour = 0;          // BGR555
    bool HasDirection = false, HasColour = false;
    u32 Times = 0;           // frames' commands that sent this same setting
    u32 FirstSite = 0;       // call site of its first command
};

// distinct (light, direction, colour) settings in the order first sent
std::vector<LightSetting> RecoverLights(const std::vector<GeometryCommand>& trace);

}

#endif // GPU3D_LIGHTRECOVERY_H
