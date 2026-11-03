#pragma once
// NVN function pointers of Pokemon Scarlet 4.0.0's main (build ID 421c5411b487eb4d049dd065fec95477): the game's
// NVN loader FUN_00cdff20 stores each function nvnDeviceGetProcAddress returns into one of these globals, and the
// game calls NVN only through them. Offsets are into main (Ghidra addresses of tools/switch/nso2elf.py's ELF).
// Read from the loader's decompilation (docs/Eden_Optimisation_Xiaomi.md section 12).
#include <cstdint>

namespace scarlet::nvn_slots {
    constexpr uintptr_t kLoader = 0x00cdff20;  // FUN_00cdff20(device, getProcAddress)

    constexpr uintptr_t kProgramSetShaders = 0x0485ccc8;
    constexpr uintptr_t kCommandBufferBindProgram = 0x0485d5e0;
    constexpr uintptr_t kCommandBufferBeginTransformFeedback = 0x0485d688;
    constexpr uintptr_t kCommandBufferEndTransformFeedback = 0x0485d690;
    constexpr uintptr_t kCommandBufferDrawArrays = 0x0485d6b0;
}
