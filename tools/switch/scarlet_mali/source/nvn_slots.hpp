#pragma once
// Where Pokemon Scarlet 4.0.0's main (build ID 421c5411b487eb4d049dd065fec95477) keeps its NVN function pointers:
// variables in main's .bss, filled by the game's NVN loader, each also reached through an entry of main's GOT.
// Offsets into main. Ghidra loads tools/switch/nso2elf.py's ELF at the image base 0x100000, so a Ghidra address is
// the offset + 0x100000 (checked 3 November: the instruction at offset 0xb51f84 loads the GOT entry 0x46e0c20,
// shown by Ghidra at 0xc51f84 and 0x47e0c20). The first three probes used Ghidra's addresses as offsets: the first's
// hook was in the wrong place and never ran, the second and third wrote past main's end (into the next module's
// code) and stopped the game (owner's logs, 3 November). Section 12 of docs/Eden_Optimisation_Xiaomi.md gives
// Ghidra's addresses.
#include <cstdint>

namespace scarlet::nvn_slots {
    constexpr uintptr_t kMainSize = 0x47d3000;

    struct Slot {
        uintptr_t variable;  // the function pointer, in main's .bss
        uintptr_t got;       // main's GOT entry holding the variable's address (checked before any write)
    };

    constexpr Slot kCommandBufferBindProgram{0x475d5e0, 0x46e0c80};
    constexpr Slot kCommandBufferBeginTransformFeedback{0x475d688, 0x46e0c10};
    constexpr Slot kCommandBufferEndTransformFeedback{0x475d690, 0x46e0c18};
    constexpr Slot kCommandBufferDrawArrays{0x475d6b0, 0x46e0c20};
}
