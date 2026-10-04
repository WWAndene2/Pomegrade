#ifndef GPU3D_PARITYORACLE_H
#define GPU3D_PARITYORACLE_H

// Parity oracle (Pomegrade, DS_ENGINE_REMAKE.md 5.6 and 5.12 step 6): the
// frame a hardware renderer just drew is drawn again by the software
// renderer, the reference for what the DS shows, and the two are compared
// pixel by pixel at the DS resolution. Run by the inspector while it records;
// any change to the OpenGL renderer can then be checked in a real game.
// Differences are expected where an enhancement changes the picture (lighting,
// relief, better polygons): turn them off to check parity itself.

#include "types.h"

#include <memory>
#include <string>

namespace melonDS
{
class GPU;
class Renderer3D;
class SoftRenderer;

class ParityOracle
{
public:
    ParityOracle() noexcept;
    ~ParityOracle();

    // emulator thread, right after `renderer` drew the frame (nothing written
    // to VRAM since: the software renderer reads the same textures)
    void Check(GPU& gpu, Renderer3D& renderer, u32 frame);
    void Clear() noexcept { Checks = 0; }

    // the report section
    [[nodiscard]] std::string Report() const;

    // a colour channel may differ by this much (rounding of 6-bit colour)
    static constexpr int Tolerance = 2;
    static constexpr int TilesX = 8, TilesY = 6; // 32x32-pixel tiles

    u32 Checks = 0;
    u32 Frame = 0;          // of the last check
    u32 Different = 0;      // pixels whose colour differs
    u32 Coverage = 0;       // drawn by one renderer only
    u32 MaxDifference = 0;  // largest channel difference (of 63)
    u32 Tiles[TilesY][TilesX] = {}; // differing pixels per tile
    u32 WorstDifferent = 0, WorstFrame = 0;

private:
    std::unique_ptr<SoftRenderer> Reference;
};

}

#endif // GPU3D_PARITYORACLE_H
