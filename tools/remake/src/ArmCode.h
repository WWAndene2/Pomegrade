#pragma once
// ARM code written by the remake's patches, for the 3DS's ARM11 (ARMv6K): no movw / movt / ubfx (ARMv6T2 and up),
// which Azahar's JIT runs but its interpreter stops at ("Decode failure"), and the hardware would not run (3 November)
#include <cstdint>
#include <vector>

namespace remake
{

// reg = value, in as few instructions as ARMv6 needs: mov then orr of each further nonzero byte (1 to 4 words)
inline void ArmLoad(std::vector<uint32_t>& c, uint32_t reg, uint32_t value)
{
    bool first = true;
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        const uint32_t byte = value >> shift & 0xFF;
        if (!byte && !(first && shift == 0)) continue;
        const uint32_t rot = shift ? (32 - shift) / 2 : 0;
        c.push_back((first ? 0xE3A00000u : 0xE3800000u | reg << 16) | reg << 12 | rot << 8 | byte); // mov / orr reg, #byte << shift
        first = false;
    }
}

}
