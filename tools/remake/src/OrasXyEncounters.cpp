#include "OrasXyEncounters.h"

#include <cstring>

namespace remake
{

namespace
{
constexpr size_t XyBytes = 392, OrasBytes = 0x194;
constexpr size_t XyLand = 0x10, XySurf = 0x100, XyEnd = 0x188;  // X/Y: land tables 0-4, then surf to hordes
constexpr size_t OrasKind0 = 0x0E, OrasKind1 = 0x3E, OrasSurf = 0x7A, OrasKind9 = 0x102;
constexpr size_t LandBytes = 12 * 4, SlotBytes = 4;
const uint8_t EmptySlot[SlotBytes] = {0, 0, 1, 1};
}

Bytes XyEncountersToOras(const Bytes& x, int xZone, bool xyEffects)
{
    if (x.empty()) return {};
    if (x.size() != XyBytes) throw FormatError("X/Y zone " + std::to_string(xZone) + ": an encounter file of " + std::to_string(x.size()) + " bytes, not 392");
    const auto land = [&](int table) { return &x[XyLand + (size_t)table * LandBytes]; };
    Bytes o(OrasBytes, 0);
    for (size_t at = OrasKind0; at + SlotBytes <= OrasBytes; at += SlotBytes) memcpy(&o[at], EmptySlot, SlotBytes);
    // kind 0: table 0; kinds 1 and 12: table 4; kinds 9-11: tables 1-3; kind 2 (no encounter starter in Omega Ruby): off
    o[0] = x[0];
    memcpy(&o[OrasKind0], land(0), LandBytes);
    o[1] = o[12] = x[4];
    memcpy(&o[OrasKind1], land(4), LandBytes);
    for (int t = 1; t <= 3; t++)
    {
        o[8 + (size_t)t] = x[(size_t)t];
        memcpy(&o[OrasKind9 + (size_t)(t - 1) * LandBytes], land(t), LandBytes);
    }
    // surf, rock smash, the three rods and hordes: the same tables in the same order (bytes 3-8, X/Y's 5-10)
    for (size_t k = 0; k < 6; k++) o[3 + k] = x[5 + k];
    memcpy(&o[OrasSurf], &x[XySurf], XyEnd - XySurf);
    if (xyEffects)
    {
        // the land and surf tables' bytes name X/Y's own effect entries, appended after Omega Ruby's 12 (oras-engine
        // --xy-wild-effects); byte 13, unused by Omega Ruby (0 in its 150 files), marks the zone X/Y's
        for (const size_t k : {0u, 1u, 3u, 9u, 10u, 11u, 12u})
            if (o[k]) o[k] = (uint8_t)(o[k] + XyEffectsAfter);
        o[13] = 1;
    }
    return o;
}

}
