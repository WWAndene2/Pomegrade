#include "GPU3D_ParityOracle.h"

#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_Soft.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace melonDS
{

ParityOracle::ParityOracle() noexcept = default;
ParityOracle::~ParityOracle() = default;

void ParityOracle::Check(GPU& gpu, Renderer3D& renderer, u32 frame)
{
    if (!Reference) Reference = std::make_unique<SoftRenderer>();

    // the hardware renderer's frame at the DS resolution (the display capture
    // path: one capture a second doesn't count as a game capturing)
    renderer.PrepareCaptureFrame();
    static u32 hw[256 * 192];
    for (int y = 0; y < 192; y++)
        memcpy(&hw[y * 256], renderer.GetLine(y), 256 * 4);

    // a frame "identical" to the previous one is not redrawn by the software
    // renderer, whose previous frame is a second old: always draw
    const bool identical = gpu.GPU3D.RenderFrameIdentical;
    gpu.GPU3D.RenderFrameIdentical = false;
    Reference->RenderFrame(gpu);
    gpu.GPU3D.RenderFrameIdentical = identical;

    Different = Coverage = MaxDifference = 0;
    memset(Tiles, 0, sizeof(Tiles));
    for (int y = 0; y < 192; y++)
    {
        const u32* ref = Reference->GetLine(y);
        for (int x = 0; x < 256; x++)
        {
            // 6 bits per colour channel, 5 bits of alpha at 24
            const u32 a = ref[x], b = hw[y * 256 + x];
            const bool drawnA = (a >> 24) & 0x1F, drawnB = (b >> 24) & 0x1F;
            if (!drawnA && !drawnB) continue;
            u32 diff = 0;
            for (int c = 0; c < 24; c += 8)
                diff = std::max<u32>(diff, (u32)std::abs((int)((a >> c) & 0x3F) - (int)((b >> c) & 0x3F)));
            if (drawnA != drawnB) Coverage++;
            else if (diff <= Tolerance) continue;
            Different++;
            Tiles[y / 32][x / 32]++;
            if (drawnA == drawnB) MaxDifference = std::max(MaxDifference, diff);
        }
    }
    Checks++;
    Frame = frame;
    if (Checks == 1 || Different > WorstDifferent) { WorstDifferent = Different; WorstFrame = Frame; }
}

std::string ParityOracle::Report() const
{
    std::string out;
    char line[160];
    out += "\n== Parity with the software renderer ==\n";
    if (!Checks)
    {
        out += "Not checked: only with a hardware renderer (OpenGL) (checked once a second while recording).\n";
        return out;
    }
    snprintf(line, sizeof(line), "%u checks. Last: %u of 49152 pixels differ (%.2f%%), %u drawn by one renderer only, largest colour difference %u/63.\n",
             Checks, Different, Different * 100.0 / 49152, Coverage, MaxDifference);
    out += line;
    snprintf(line, sizeof(line), "Worst check: %u pixels. Enhancements (lighting, relief, better polygons) differ by design.\n", WorstDifferent);
    out += line;
    out += "Differing pixels per 32x32 tile of the last check (. none, 1-9 tenths, # all):\n";
    for (int ty = 0; ty < TilesY; ty++)
    {
        out += "    ";
        for (int tx = 0; tx < TilesX; tx++)
        {
            u32 n = Tiles[ty][tx];
            out += n == 0 ? '.' : n >= 1024 ? '#' : (char)('0' + std::max<u32>(1, n * 10 / 1024));
        }
        out += "\n";
    }
    return out;
}

}
