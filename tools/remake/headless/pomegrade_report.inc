// Appended to azahar/src/citra_libretro/citra_libretro.cpp by the Remake headless workflow only (never part of the app or the
// repository's Azahar sources): retro_host calls it when the game has not reached the field, to see where the game is stuck.
// For every thread of the game: its state, PC, LR and SP (the running one's from its core, the others' from their saved
// context), and the words of its stack that look like code addresses (a call chain, read without frame pointers).
// A thread in the game's fatal-error loop (code 0x11EF3C-0x11EF64, ORAS_LITTLEROOT.md 0) also gets its registers and its
// raw stack, to rebuild the frames' saved registers and find the object whose check failed; pomegrade_peek dumps memory.
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"

extern "C" RETRO_API void pomegrade_report(void (*out)(const char* line)) {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn()) {
        out("pomegrade_report: not powered on");
        return;
    }
    auto& kernel = system.Kernel();
    auto& memory = system.Memory();
    char line[512];
    static const char* const states[] = {"running", "ready", "wait-arbiter", "wait-sleep", "wait-ipc", "wait-synch-any",
                                         "wait-synch-all", "wait-hle", "dormant", "dead"};
    for (u32 core = 0; core < system.GetNumCores(); core++) {
        const auto& manager = kernel.GetThreadManager(core);
        const auto current = manager.GetCurrentThread();
        for (const auto& thread : manager.GetThreadList()) {
            const auto process = thread->owner_process.lock();
            if (!process || process->codeset == nullptr || process->codeset->program_id != 0x000400000011C400) continue; // Omega Ruby
            const bool live = thread.get() == current;
            const auto& cpu = system.GetCore(core);
            const u32 pc = live ? cpu.GetPC() : thread->context.GetProgramCounter();
            const u32 lr = live ? cpu.GetReg(14) : thread->context.GetLinkRegister();
            const u32 sp = live ? cpu.GetReg(13) : thread->context.GetStackPointer();
            const int status = static_cast<int>(thread->status);
            snprintf(line, sizeof line, "thread %u core %u %s%s pc %08X lr %08X sp %08X waits on %zu object(s)", thread->thread_id, core,
                     status >= 0 && status < 10 ? states[status] : "?", live ? " (on the core)" : "", pc, lr, sp,
                     thread->wait_objects.size());
            out(line);
            std::string chain = "  stack code addresses:";
            int found = 0;
            for (u32 at = sp; at < sp + 0x800 && found < 24; at += 4) {
                if (!memory.IsValidVirtualAddress(*process, at)) break;
                const u32 word = memory.Read32(*process, at);
                // the .code (0x100000-0x630000) and the code modules loaded above it (DllField at 0x6F3000 in the phone's log)
                if ((word >= 0x100000 && word < 0x630000) || (word >= 0x6F0000 && word < 0x900000)) {
                    char item[24];
                    snprintf(item, sizeof item, " %08X@+%X", word, at - sp);
                    chain += item;
                    found++;
                }
            }
            out(chain.c_str());
            if (chain.find("0011EF60") == std::string::npos && (pc < 0x11EF3C || pc > 0x11EF64)) continue;
            std::string regs = "  registers:";
            for (int r = 0; r < 16; r++) {
                char item[24];
                snprintf(item, sizeof item, " r%d=%08X", r, live ? cpu.GetReg(r) : thread->context.cpu_registers[r]);
                regs += item;
            }
            out(regs.c_str());
            for (u32 at = sp; at < sp + 0x400; at += 0x20) {
                std::string row;
                char item[24];
                snprintf(item, sizeof item, "  stack +%03X:", at - sp);
                row = item;
                for (u32 w = at; w < at + 0x20 && memory.IsValidVirtualAddress(*process, w); w += 4) {
                    snprintf(item, sizeof item, " %08X", memory.Read32(*process, w));
                    row += item;
                }
                out(row.c_str());
            }
        }
    }
}

// the game's memory from address for length bytes, 32 bytes a line, words as the game reads them
extern "C" RETRO_API void pomegrade_peek(u32 address, u32 length, void (*out)(const char* line)) {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn()) return;
    for (const auto& thread : system.Kernel().GetThreadManager(0).GetThreadList()) {
        const auto process = thread->owner_process.lock();
        if (!process || process->codeset == nullptr || process->codeset->program_id != 0x000400000011C400) continue;
        auto& memory = system.Memory();
        for (u32 at = address & ~3u; at < address + length; at += 0x20) {
            char line[512];
            int used = snprintf(line, sizeof line, "  mem %08X:", at);
            for (u32 w = at; w < at + 0x20 && used < 480; w += 4)
                used += memory.IsValidVirtualAddress(*process, w) ? snprintf(line + used, sizeof line - used, " %08X", memory.Read32(*process, w))
                                                                  : snprintf(line + used, sizeof line - used, " --------");
            out(line);
        }
        return;
    }
}
