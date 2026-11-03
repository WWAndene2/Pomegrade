// Pomegrade's Mali version of Pokemon Scarlet: an exlaunch module (subsdk9) loaded beside the game by Eden.
//
// This first build is a probe and changes nothing the game draws. It checks, on the owner's phone, the chain the
// skinning replacement needs (docs/Eden_Optimisation_Xiaomi.md section 12): the module is loaded, the hook on the
// game's NVN loader runs, the program and transform feedback calls can be intercepted, and which programs the
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
    original = *slot;
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

}  // namespace

// The game's NVN loader: each time it fills the function pointers, a few are swapped again for the probe's.
HOOK_DEFINE_TRAMPOLINE(NvnLoader) {
    static void Callback(void* device, void* get_proc_address) {
        Orig(device, get_proc_address);
        SwapAll("after the NVN loader ran");
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();
    NvnLoader::InstallAtOffset(scarlet::nvn_slots::kLoader);
    Log("[scarlet_mali] module loaded (main at %p)", reinterpret_cast<void*>(MainBase()));
    // Under Eden the module starts after the game has set NVN up (owner's log, 3 November: modules loaded at 5.4 s,
    // this module's start at 11.1 s, no loader call after it), so the pointers already filled are swapped now. A
    // pointer is one aligned 64-bit store, so a thread calling through it gets the old function or the new one.
    if (*Slot<DrawArraysFn>(scarlet::nvn_slots::kCommandBufferDrawArrays) != nullptr) SwapAll("at start (NVN already set up)");
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
