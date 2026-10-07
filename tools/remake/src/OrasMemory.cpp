#include "OrasMemory.h"

namespace remake
{

// the ARM11 local system caps start at 0x200 of the extended header (after the 0x200-byte system control info); their flags0
// byte (ideal processor, affinity mask, system mode) is at 0x0E of them
static constexpr size_t LocalCaps = 0x200, Flags0 = LocalCaps + 0x0E;

Bytes ExHeaderWithSystemMode(const Bytes& exheader, OrasMemoryMode mode)
{
    if (exheader.size() != 0x800) throw FormatError("the extended header is not 0x800 bytes");
    // a decrypted header names its program in the system info's jump id (0x1C8, after its savedata size) and the local caps
    // (0x200): Azahar warns
    // when they differ, as for an encrypted one
    if (!std::equal(exheader.begin() + 0x1C8, exheader.begin() + 0x1D0, exheader.begin() + LocalCaps))
        throw FormatError("the extended header is not decrypted (its jump id and program id differ)");
    Bytes out = exheader;
    out[Flags0] = (uint8_t)((out[Flags0] & 0x0F) | ((unsigned)mode << 4));
    return out;
}

uint32_t MemoryModeBytes(OrasMemoryMode mode)
{
    switch (mode)
    {
    case OrasMemoryMode::Prod64: return 64u << 20;
    case OrasMemoryMode::Dev1_96: return 96u << 20;
    case OrasMemoryMode::Dev2_80: return 80u << 20;
    case OrasMemoryMode::Dev3_72: return 72u << 20;
    }
    throw FormatError("unknown memory mode");
}

}
