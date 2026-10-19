#include "OrasXyEncounters.h"

#include "BinLinker.h"
#include "NitroCompression.h"

#include <cstdio>
#include <cstring>

namespace remake
{

namespace
{
constexpr size_t XyBytes = 392, OrasBytes = 260;
constexpr size_t XyLand = 0x10, XySurf = 0x100, XyEnd = 0x188;  // X/Y: land tables 0-4, then surf to hordes
constexpr size_t OrasKind0 = 0x0E, OrasKind1 = 0x3E, OrasSurf = 0x7A, OrasEnd = 0x102;
constexpr size_t LandSlots = 12, SlotBytes = 4;
const uint8_t EmptySlot[SlotBytes] = {0, 0, 1, 1};
}

std::array<size_t, 4> XyLandTiles(int xZone, const OrasMatrix& xMatrix, const Garc& xPieces)
{
    std::array<size_t, 4> tiles{};
    for (int py = 0; py < xMatrix.Height; py++)
        for (int px = 0; px < xMatrix.Width; px++)
        {
            const uint16_t piece = xMatrix.Pieces.at((size_t)py * xMatrix.Width + px);
            if (piece == OrasMatrix::None) continue;
            const Bytes plain = IsLzCompressed(xPieces.Sub(piece)) ? LzDecompress(xPieces.Sub(piece)) : xPieces.Sub(piece);
            const BinLinker gr = BinLinker::Read(plain, "GR");
            const Bytes& grid = gr.Files.at(0);
            if (grid.size() < 4 + 1600 * 4) throw FormatError("X piece " + std::to_string(piece) + ": a tile grid of " + std::to_string(grid.size()) + " bytes");
            for (int ty = 0; ty < 40; ty++)
                for (int tx = 0; tx < 40; tx++)
                {
                    const size_t block = (size_t)(py * 4 + ty / 10) * xMatrix.Width * 4 + (size_t)(px * 4 + tx / 10);
                    if (!xMatrix.Zones.empty() && xMatrix.Zones.at(block) != xZone) continue;
                    const uint8_t kind = grid[4 + 4 * (size_t)(ty * 40 + tx) + 3];
                    if (kind == 0x20 || kind == 0x24 || kind == 0x25) tiles[0]++;
                    else if (kind >= 0x21 && kind <= 0x23) tiles[kind - 0x20]++;
                }
        }
    return tiles;
}

Bytes XyEncountersToOras(const Bytes& x, const std::array<size_t, 4>& landTiles, int xZone, std::vector<std::string>& log)
{
    if (x.empty()) return {};
    if (x.size() != XyBytes) throw FormatError("X/Y zone " + std::to_string(xZone) + ": an encounter file of " + std::to_string(x.size()) + " bytes, not 392");
    const auto on = [&](int table) {
        for (size_t s = 0; s < LandSlots; s++)
            if (memcmp(&x[XyLand + (table * LandSlots + s) * SlotBytes], EmptySlot, SlotBytes) != 0) return true;
        return x[(size_t)table] != 0;
    };
    // kind 0: of X/Y's tables 0-3 switched on, the one with the most tiles (table 0 first on a tie)
    int kind0 = -1;
    std::vector<int> left;
    for (int t = 0; t < 4; t++)
    {
        if (!on(t)) continue;
        if (kind0 < 0 || landTiles[(size_t)t] > landTiles[(size_t)kind0]) { if (kind0 >= 0) left.push_back(kind0); kind0 = t; }
        else left.push_back(t);
    }
    for (const int t : left)
    {
        char line[160];
        snprintf(line, sizeof line, "X/Y zone %d: its land table %d (%zu tiles) left out, its tiles taking table %d's Pokemon", xZone, t, landTiles[(size_t)t], kind0);
        log.push_back(line);
    }

    Bytes o(OrasBytes, 0);
    for (size_t at = OrasKind0; at < OrasEnd; at += SlotBytes) memcpy(&o[at], EmptySlot, SlotBytes);
    if (kind0 >= 0)
    {
        o[0] = x[(size_t)kind0];
        memcpy(&o[OrasKind0], &x[XyLand + kind0 * LandSlots * SlotBytes], LandSlots * SlotBytes);
    }
    o[1] = x[4];
    memcpy(&o[OrasKind1], &x[XyLand + 4 * LandSlots * SlotBytes], LandSlots * SlotBytes);
    // kind 2 (+0x6E, no encounter starter found in Omega Ruby): none in X/Y, left empty and off (byte 2 zero)
    // surf, rock smash, the three rods and hordes: the same tables in the same order (bytes 3-8, X/Y's 5-10)
    for (int k = 0; k < 6; k++) o[3 + (size_t)k] = x[5 + (size_t)k];
    memcpy(&o[OrasSurf], &x[XySurf], XyEnd - XySurf);
    return o;
}

}
