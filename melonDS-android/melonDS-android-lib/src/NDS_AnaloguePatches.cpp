#include "NDS_AnaloguePatches.h"

#include <cmath>
#include <cstring>

namespace melonDS
{

namespace
{

// Dragon Quest Monsters: Joker (Europe, AJRP). Walking in the field: at
// 021BC130 the game reads the held keys and switches on the 8 D-pad
// directions, setting r4 = walking direction relative to the camera in
// degrees x 4096 (0 towards the camera, 90 right, 180 away, -90 left) and
// r0 = a turn of the camera (-0x2800 walking right, 0x2800 left, half on
// diagonals, 0 straight ahead or back), then goes on at 021BC220, which
// turns the hero towards r4 (at most 30 degrees per update) and walks.
// The patch keeps the key test and replaces the switch with a load of both
// values from the stick registers.
void DQMJokerRegisters(float x, float y, s32 out[2])
{
    double degrees = std::atan2(x, -y) * 180.0 / M_PI;
    out[0] = (s32)std::lround(degrees * 4096.0);
    // the game's camera turn, continuous between its 8 values: full sideways,
    // half at 45 degrees, none ahead or back
    double side = 1.0 - std::fabs(std::fabs(degrees) - 90.0) / 90.0;
    out[1] = (s32)std::lround((degrees >= 0 ? -1.0 : 1.0) * 0x2800 * side);
}

const AnaloguePatch Patches[] = {
    {
        {'A', 'J', 'R', 'P'}, 0x021BC144,
        {
            0xE1D080B0, // ldrh r8, [r0]        held keys
            0xE1D060B2, // ldrh r6, [r0, #2]    pressed keys
            0xE21800F0, // ands r0, r8, #0xF0   a direction held?
            0x0A00004D, // beq 021BC28C         no: standing
            0xE3500050, // cmp r0, #0x50        the switch on the 8 directions
            0xCA000009, // bgt 021BC184
            0xAA000021, // bge 021BC1E8
            0xE3500020, // cmp r0, #0x20
            0xCA000003, // bgt 021BC178
        },
        {
            0xE1D080B0, // ldrh r8, [r0]
            0xE1D060B2, // ldrh r6, [r0, #2]
            0xE21800F0, // ands r0, r8, #0xF0
            0x0A00004D, // beq 021BC28C
            0xE59F1008, // ldr r1, =AnalogueStick::RegisterBase
            0xE5914000, // ldr r4, [r1]         direction
            0xE5910004, // ldr r0, [r1, #4]     camera turn
            0xEA00002E, // b 021BC220
            0x04FFF600, // AnalogueStick::RegisterBase
        },
        DQMJokerRegisters,
    },
};

}

const AnaloguePatch* FindAnaloguePatch(const char gameCode[4])
{
    for (const AnaloguePatch& patch : Patches)
        if (!std::memcmp(patch.GameCode, gameCode, 4))
            return &patch;
    return nullptr;
}

}
