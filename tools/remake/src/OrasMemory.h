#ifndef REMAKE_ORASMEMORY_H
#define REMAKE_ORASMEMORY_H

// More memory for Omega Ruby (ORAS_ENGINE.md 4.2): the game declares the 3DS's standard 64 MB application mode in its
// extended header (system_mode 0) and asks for a fixed 43.3 MB linear heap at boot (the word 0x2B48000 at 0x106508, read by
// FUN_00106448); its main heap is that linear heap less 0xE88000, so a larger linear heap grows the main heap with it. A mod
// gives it more: exheader.bin with a larger system mode, and code.ips raising the linear heap word (both written by
// BuildEngineMod, OrasEngine.h).

#include "N3dsRom.h"

#include <string>
#include <vector>

namespace remake
{

// the 3DS application memory modes of the Old 3DS (system_mode, the extended header's ARM11 local caps flags0 bits 4-7) and
// the application memory each gives (Azahar's Kernel::MemoryMode)
enum class OrasMemoryMode { Prod64 = 0, Dev1_96 = 2, Dev2_80 = 3, Dev3_72 = 4 };

// the application memory a mode gives (bytes)
uint32_t MemoryModeBytes(OrasMemoryMode mode);

// the extended header with its system mode changed (checked: a 0x800-byte header naming the program in its local caps)
Bytes ExHeaderWithSystemMode(const Bytes& exheader, OrasMemoryMode mode);

}

#endif // REMAKE_ORASMEMORY_H
