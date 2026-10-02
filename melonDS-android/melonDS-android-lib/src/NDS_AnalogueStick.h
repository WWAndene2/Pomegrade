#ifndef NDS_ANALOGUESTICK_H
#define NDS_ANALOGUESTICK_H

// Analogue movement (Pomegrade): the DS has no stick, so a game walks in the
// 8 directions of its D-pad. For the games listed in NDS_AnaloguePatches, the
// code that turns the D-pad into a walking direction is patched to read the
// stick's direction from a register only Pomegrade has (RegisterBase), the
// rest of the game unchanged. Off by default; unpatched games never read it.

#include "types.h"

#include <atomic>

namespace melonDS
{
class NDS;
struct AnaloguePatch;

class AnalogueStick
{
public:
    // Pomegrade-only I/O registers, read by patched game code: two words whose
    // meaning the patch defines (see AnaloguePatch::Registers)
    static constexpr u32 RegisterBase = 0x04FFF600;
    static constexpr u32 RegisterEnd = RegisterBase + 8;
    // below this tilt the stick counts as released (the D-pad decides)
    static constexpr float DeadZone = 0.2f;

    void SetEnabled(bool enabled) noexcept { Enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const noexcept { return Enabled; }

    // x to the right, y up, each -1 to 1; (0, 0) when released. Any thread.
    void SetPosition(float x, float y) noexcept;

    // once per frame, before the frame runs: patches the game's movement code
    // when it is in memory (or restores it when disabled)
    void RunFrame(NDS& nds);

    // keyinput: the KEYINPUT register (active low), for the D-pad fallback
    [[nodiscard]] u32 Read32(u32 addr, u32 keyinput) const noexcept;

private:
    std::atomic<bool> Enabled {false};
    std::atomic<u32> Position {0}; // x and y as s16, x in the low half
    const AnaloguePatch* ActivePatch = nullptr; // patched into memory this frame
};

}

#endif // NDS_ANALOGUESTICK_H
