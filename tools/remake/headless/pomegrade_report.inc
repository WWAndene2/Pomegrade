// Appended to azahar/src/citra_libretro/citra_libretro.cpp by the Remake headless workflow only (never part of the app or the
// repository's Azahar sources): retro_host calls it when the game has not reached the field, to see where the game is stuck.
// For every thread of the game: its state, PC, LR and SP (the running one's from its core, the others' from their saved
// context), and the words of its stack that look like code addresses (a call chain, read without frame pointers).
// A thread in the game's fatal-error loop (code 0x11EF3C-0x11EF64, ORAS_LITTLEROOT.md 0) also gets its registers and its
// raw stack, to rebuild the frames' saved registers and find the object whose check failed; pomegrade_peek dumps memory.
#include "common/settings.h"
#ifdef ENABLE_OPENGL
#include "video_core/renderer_opengl/gl_pathtracer.h"
#endif
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#ifdef ENABLE_GDBSTUB
#include "core/gdbstub/gdbstub.h"
#endif

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

// the emulator's GDB stub opened on a port (script command "gdb PORT"): breakpoints, watchpoints and memory on the running
// game from gdb-multiarch (target remote :PORT), to read the engine's code at work. Only in a core built with ENABLE_GDBSTUB
// the Remaster preset (0 off, 1 v7, 2 v9 with the path tracer, 3 custom) from now on: read each frame by the renderer, so
// no core option update (which resets the libretro frontend's layout: the screens came out black)
extern "C" RETRO_API void pomegrade_remaster(u32 preset) {
    Settings::values.remaster_preset = preset;
}

// the path tracer's sun at that hour of the owner's table (negative: the game's clock again), to render a time of day
extern "C" RETRO_API void pomegrade_sun_hour(float hour) {
#ifdef ENABLE_OPENGL
    OpenGL::PathTracerGL::SetTestHour(hour);
#endif
}

extern "C" RETRO_API bool pomegrade_gdb(u16 port) {
#ifdef ENABLE_GDBSTUB
    GDBStub::SetServerPort(port);
    GDBStub::ToggleServer(true);
    return true;
#else
    return false;
#endif
}

// the game's memory from address for length bytes written raw to path (unmapped pages as zeros): the code modules as the
// loader relocated and linked them, for Ghidra (script command "dump ADDRESS LENGTH FILE"); returns the bytes that were mapped
extern "C" RETRO_API u32 pomegrade_dump(u32 address, u32 length, const char* path) {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn()) return 0;
    for (const auto& thread : system.Kernel().GetThreadManager(0).GetThreadList()) {
        const auto process = thread->owner_process.lock();
        if (!process || process->codeset == nullptr || process->codeset->program_id != 0x000400000011C400) continue;
        auto& memory = system.Memory();
        std::vector<u8> data(length, 0);
        u32 mapped = 0;
        for (u32 page = 0; page < length; page += 0x1000) {
            if (!memory.IsValidVirtualAddress(*process, address + page)) continue;
            memory.ReadBlock(*process, address + page, data.data() + page, std::min<u32>(0x1000, length - page));
            mapped += std::min<u32>(0x1000, length - page);
        }
        if (FILE* f = fopen(path, "wb")) { fwrite(data.data(), 1, data.size(), f); fclose(f); }
        return mapped;
    }
    return 0;
}

// Code coverage (script commands "trace on" / "trace off FILE", with the interpreter: POMEGRADE_INTERPRETER=1): the address
// of every block of ARM code the game enters between the two, hooked at the interpreter's dispatch by patch_core.py; the
// sorted distinct addresses written to FILE, one per line. What an activity runs (entering a zone, a script, a battle) is
// the difference between two traces: ORAS_ENGINE.md 6
#include <algorithm>
#include <unordered_set>
bool pomegrade_trace_on = false;
static std::unordered_set<u32> pomegrade_trace_blocks;
extern "C" void pomegrade_trace_block(u32 pc) { pomegrade_trace_blocks.insert(pc); }
extern "C" RETRO_API u32 pomegrade_trace(int on, const char* path) {
    if (on) { pomegrade_trace_blocks.clear(); pomegrade_trace_on = true; return 0; }
    pomegrade_trace_on = false;
    std::vector<u32> blocks(pomegrade_trace_blocks.begin(), pomegrade_trace_blocks.end());
    std::sort(blocks.begin(), blocks.end());
    if (FILE* f = fopen(path, "w")) { for (u32 b : blocks) fprintf(f, "%08X\n", b); fclose(f); }
    return (u32)blocks.size();
}

// lights on | lights off FILE: the light state of each draw in between (pica_lights.inc), each distinct one with its count,
// then the vertex shader's float uniforms, each register's distinct values (32 at most) with their counts
#include <map>
#include <string>
extern bool pomegrade_lights_on;
extern unsigned pomegrade_light_draws;
extern std::map<std::string, unsigned> pomegrade_light_states;
extern std::map<std::string, unsigned> pomegrade_uniform_values[96];
extern "C" RETRO_API u32 pomegrade_lights(int on, const char* path) {
    if (on) {
        pomegrade_light_draws = 0;
        pomegrade_light_states.clear();
        for (auto& m : pomegrade_uniform_values) m.clear();
        pomegrade_lights_on = true;
        return 0;
    }
    pomegrade_lights_on = false;
    if (FILE* f = fopen(path, "w")) {
        fprintf(f, "draws %u\n", pomegrade_light_draws);
        for (const auto& [state, count] : pomegrade_light_states) fprintf(f, "state %u: %s\n", count, state.c_str());
        for (int i = 0; i < 96; i++)
            for (const auto& [value, count] : pomegrade_uniform_values[i]) fprintf(f, "uniform %d %u: %s\n", i, count, value.c_str());
        fclose(f);
    }
    return pomegrade_light_draws;
}

// poke ADDRESS VALUE: one word of the game's memory written, to see what a value does while the game runs (a value changed
// in memory instead of in a file: ORAS_ENGINE.md 2, the light). Returns false when the address is not mapped.
extern "C" RETRO_API bool pomegrade_poke(u32 address, u32 value) {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn()) return false;
    for (const auto& thread : system.Kernel().GetThreadManager(0).GetThreadList()) {
        const auto process = thread->owner_process.lock();
        if (!process || process->codeset == nullptr || process->codeset->program_id != 0x000400000011C400) continue;
        auto& memory = system.Memory();
        if (!memory.IsValidVirtualAddress(*process, address)) return false;
        memory.WriteBlock(*process, address, &value, 4);
        return true;
    }
    return false;
}

// writers LO HI [PCLO PCHI] | writers off FILE: every write of the game (interpreter only: POMEGRADE_INTERPRETER=1) into [LO, HI) in
// between, with the instruction's address and the return address (patch_core.py, armstate.cpp): who fills a structure
u32 pomegrade_writers_lo = 0, pomegrade_writers_hi = 0;
static u32 pomegrade_writers_pclo = 0, pomegrade_writers_pchi = 0xFFFFFFFF; // only the writes of this code (all by default)
static std::vector<std::array<u32, 4>> pomegrade_writes;
extern "C" void pomegrade_writer(u32 address, u32 value, u32 pc, u32 lr) {
    if (pc < pomegrade_writers_pclo || pc >= pomegrade_writers_pchi) return;
    if (pomegrade_writes.size() < 100000) pomegrade_writes.push_back({address, value, pc, lr});
}
extern "C" RETRO_API void pomegrade_writers_code(u32 lo, u32 hi) { pomegrade_writers_pclo = lo; pomegrade_writers_pchi = hi; }
extern "C" RETRO_API u32 pomegrade_writers(u32 lo, u32 hi, const char* path) {
    if (path == nullptr) { pomegrade_writes.clear(); pomegrade_writers_lo = lo; pomegrade_writers_hi = hi; return 0; }
    pomegrade_writers_lo = pomegrade_writers_hi = 0;
    if (FILE* f = fopen(path, "w")) {
        for (const auto& w : pomegrade_writes) fprintf(f, "%08X %08X pc %08X lr %08X\n", w[0], w[1], w[2], w[3]);
        fclose(f);
    }
    return (u32)pomegrade_writes.size();
}
