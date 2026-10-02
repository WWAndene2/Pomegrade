#ifndef NDS_ANALOGUEPATCHES_H
#define NDS_ANALOGUEPATCHES_H

// The games analogue movement supports (Pomegrade), see NDS_AnalogueStick.h.
// Each patch replaces a few instructions of one exact game version; it is
// written only over the original instructions, compared word for word.

#include "types.h"

#include <array>

namespace melonDS
{

struct AnaloguePatch
{
    static constexpr int Length = 9; // words

    char GameCode[4];
    u32 Address;
    std::array<u32, Length> Original;
    std::array<u32, Length> Patched;
    // the two register words for a stick direction (x right, y up, not zero)
    void (*Registers)(float x, float y, s32 out[2]);
};

// the patch for a game code, nullptr when the game isn't supported
const AnaloguePatch* FindAnaloguePatch(const char gameCode[4]);

}

#endif // NDS_ANALOGUEPATCHES_H
