// Pomegrade's Mali version of Pokemon Scarlet: an exlaunch module (subsdk9) loaded beside the game by Eden.
//
// This first build is a probe and changes nothing the game draws. It checks, on the owner's phone, the chain the
// skinning replacement needs (docs/Eden_Optimisation_Xiaomi.md section 12): the module is loaded, the game's NVN
// function pointers can be swapped once its loader has filled them, the program and transform feedback calls can be
// intercepted, and which programs the
// transform feedback draws use. It logs through svcOutputDebugString, which Eden writes to its log as
// "Debug_Emulated" lines prefixed "[scarlet_mali]".
#include "lib.hpp"
#include "nvn_slots.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace {

void Log(const char* format, ...) {
    char buffer[384];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length > 0) {
        svcOutputDebugString(buffer, static_cast<u64>(length < static_cast<int>(sizeof(buffer)) ? length : sizeof(buffer) - 1));
    }
}

uintptr_t MainBase() { return exl::util::modules::GetTargetStart(); }

template <typename T>
T* Slot(uintptr_t offset) { return reinterpret_cast<T*>(MainBase() + offset); }

// NVN calls, forwarded with their arguments as the game passes them (every argument fits in a register).
using SetShadersFn = u8 (*)(void* program, int count, const void* shader_data);
using BindProgramFn = void (*)(void* cmd, const void* program, int stages);
using BeginXfbFn = void (*)(void* cmd, u64 address);
using EndXfbFn = void (*)(void* cmd, u64 address);
using DrawArraysFn = void (*)(void* cmd, int primitive, int first, int count);

SetShadersFn g_set_shaders;
BindProgramFn g_bind_program;
BeginXfbFn g_begin_xfb;
EndXfbFn g_end_xfb;
DrawArraysFn g_draw_arrays;

// The game records each draw into its own command buffer on one thread (FUN_00c4fa44), so the state of a command
// buffer is the program last bound and whether transform feedback is active. A small table keyed by the command
// buffer's address holds it; a full table only loses probe statistics.
struct CommandState {
    std::atomic<void*> cmd;
    const void* program;
    bool in_xfb;
};
constexpr size_t kStates = 1024;
CommandState g_states[kStates];

CommandState* StateOf(void* cmd) {
    size_t index = (reinterpret_cast<uintptr_t>(cmd) >> 4) % kStates;
    for (size_t probe = 0; probe < 16; ++probe, index = (index + 1) % kStates) {
        void* owner = g_states[index].cmd.load(std::memory_order_acquire);
        if (owner == cmd) return &g_states[index];
        if (owner == nullptr) {
            void* expected = nullptr;
            if (g_states[index].cmd.compare_exchange_strong(expected, cmd) || expected == cmd) return &g_states[index];
        }
    }
    return nullptr;
}

// Programs seen in transform feedback draws, with their draw and vertex counts.
struct ProgramStats {
    std::atomic<const void*> program;
    std::atomic<u64> draws;
    std::atomic<u64> vertices;
};
constexpr size_t kPrograms = 64;
ProgramStats g_programs[kPrograms];
std::atomic<u64> g_xfb_draws;
std::atomic<u32> g_single_stage_programs;

void CountXfbDraw(const void* program, int count) {
    for (auto& stats : g_programs) {
        const void* owner = stats.program.load(std::memory_order_acquire);
        if (owner == nullptr) {
            const void* expected = nullptr;
            if (!stats.program.compare_exchange_strong(expected, program) && expected != program) continue;
        } else if (owner != program) {
            continue;
        }
        stats.draws.fetch_add(1, std::memory_order_relaxed);
        stats.vertices.fetch_add(static_cast<u64>(count), std::memory_order_relaxed);
        break;
    }
    u64 total = g_xfb_draws.fetch_add(1, std::memory_order_relaxed) + 1;
    if (total % 20000 == 0) {
        Log("[scarlet_mali] %llu transform feedback draws so far, by program:", static_cast<unsigned long long>(total));
        for (auto& stats : g_programs) {
            const void* program_seen = stats.program.load(std::memory_order_acquire);
            if (program_seen == nullptr) break;
            Log("[scarlet_mali]   program %p: %llu draws, %llu vertices", program_seen,
                static_cast<unsigned long long>(stats.draws.load()), static_cast<unsigned long long>(stats.vertices.load()));
        }
    }
}

u8 SetShaders(void* program, int count, const void* shader_data) {
    // A program with one stage and transform feedback outputs is a capture pass (skinning and the other
    // deformers, section 12); its NVN control data starts with 0x98761234 and lists the captured outputs from
    // byte 220 (read from system/skinning's BNSH). Logged here so the probe shows which programs those are.
    if (count == 1 && shader_data != nullptr) {
        const u8* control = *reinterpret_cast<const u8* const*>(static_cast<const u8*>(shader_data) + 8);
        if (control != nullptr) {
            u32 magic = *reinterpret_cast<const u32*>(control);
            u32 first_output = *reinterpret_cast<const u32*>(control + 220);
            u32 seen = g_single_stage_programs.fetch_add(1, std::memory_order_relaxed);
            if (seen < 200) {
                Log("[scarlet_mali] one-stage program %p: control magic %08x, first captured output %08x", program, magic,
                    first_output);
            }
        }
    }
    return g_set_shaders(program, count, shader_data);
}

void BindProgram(void* cmd, const void* program, int stages) {
    if (CommandState* state = StateOf(cmd)) state->program = program;
    g_bind_program(cmd, program, stages);
}

void BeginXfb(void* cmd, u64 address) {
    if (CommandState* state = StateOf(cmd)) state->in_xfb = true;
    g_begin_xfb(cmd, address);
}

void EndXfb(void* cmd, u64 address) {
    if (CommandState* state = StateOf(cmd)) state->in_xfb = false;
    g_end_xfb(cmd, address);
}

void DrawArrays(void* cmd, int primitive, int first, int count) {
    if (CommandState* state = StateOf(cmd); state != nullptr && state->in_xfb) CountXfbDraw(state->program, count);
    g_draw_arrays(cmd, primitive, first, count);
}

template <typename Fn>
void Swap(uintptr_t offset, Fn& original, Fn replacement) {
    Fn* slot = Slot<Fn>(offset);
    Fn current = *slot;
    // a slot still holding the module's function is left as is: taking it for the game's would make the function
    // call itself
    if (current == nullptr || current == replacement) return;
    original = current;
    *slot = replacement;
}

void SwapAll(const char* when) {
    Swap(scarlet::nvn_slots::kProgramSetShaders, g_set_shaders, SetShaders);
    Swap(scarlet::nvn_slots::kCommandBufferBindProgram, g_bind_program, BindProgram);
    Swap(scarlet::nvn_slots::kCommandBufferBeginTransformFeedback, g_begin_xfb, BeginXfb);
    Swap(scarlet::nvn_slots::kCommandBufferEndTransformFeedback, g_end_xfb, EndXfb);
    Swap(scarlet::nvn_slots::kCommandBufferDrawArrays, g_draw_arrays, DrawArrays);
    Log("[scarlet_mali] 5 NVN calls intercepted %s (probe build, nothing drawn differently)", when);
}


// The pointers are swapped by a thread of the module once the game has filled them, not by a hook on the game's
// loader: under Eden, the first probe's trampoline hook on FUN_00cdff20 never ran, and the second's (owner's log,
// 3 November: the module started at 19.4 s with the pointers still empty) stopped the game at 68.7 s, likely when
// the game reached the patched code. The module now changes no code of the game, only these data pointers.
alignas(16) u8 g_watch_stack[0x4000];

void WatchNvnSetup(void*) {
    // DrawArrays is among the last functions the loader fills; checked every 50 ms until it is, then every 500 ms in
    // case the loader runs again (it has two callers) and puts the game's own functions back: they are swapped again
    for (;;) {
        DrawArraysFn current = *Slot<DrawArraysFn>(scarlet::nvn_slots::kCommandBufferDrawArrays);
        if (current != nullptr && current != DrawArrays) {
            svcSleepThread(50'000'000);  // the loader fills the rest of its 532 pointers in the meantime
            SwapAll(g_draw_arrays == nullptr ? "once the game had set NVN up" : "again, after the game set NVN up again");
        }
        svcSleepThread(current == nullptr ? 50'000'000 : 500'000'000);
    }
}

}  // namespace

extern "C" void exl_main(void* x0, void* x1) {
    Log("[scarlet_mali] module loaded (main at %p)", reinterpret_cast<void*>(MainBase()));
    Handle thread;
    if (R_SUCCEEDED(svcCreateThread(&thread, reinterpret_cast<void*>(&WatchNvnSetup), nullptr,
                                    g_watch_stack + sizeof(g_watch_stack), 0x2C, -2)) &&
        R_SUCCEEDED(svcStartThread(thread))) {
        Log("[scarlet_mali] waiting for the game to set NVN up");
    } else {
        Log("[scarlet_mali] could not start the watch thread: nothing intercepted");
    }
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
