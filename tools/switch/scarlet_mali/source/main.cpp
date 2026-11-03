// Pomegrade's Mali version of Pokemon Scarlet: an exlaunch module (subsdk9) loaded beside the game by Eden.
//
// This first build is a probe and changes nothing the game draws. It checks, on the owner's phone, the chain the
// skinning replacement needs (docs/Eden_Optimisation_Xiaomi.md section 12): the module is loaded, the game's NVN
// function pointers can be swapped once its loader has filled them, the program bind and transform feedback calls
// can be intercepted, and which programs the transform feedback draws use. It logs through svcOutputDebugString,
// which Eden writes to its log as "Debug_Emulated" lines prefixed "[scarlet_mali]".
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

// The variable holding an NVN function (nvn_slots.hpp), or null unless main's GOT entry for it holds exactly its
// address: a check that this main is the build the offsets were read from, made before anything is written.
template <typename T>
T* Slot(const scarlet::nvn_slots::Slot& slot) {
    uintptr_t expected = MainBase() + slot.variable;
    if (*reinterpret_cast<const uintptr_t*>(MainBase() + slot.got) != expected) return nullptr;
    return reinterpret_cast<T*>(expected);
}

// NVN calls, forwarded with their arguments as the game passes them (every argument fits in a register).
using BindProgramFn = void (*)(void* cmd, const void* program, int stages);
using BeginXfbFn = void (*)(void* cmd, u64 address);
using EndXfbFn = void (*)(void* cmd, u64 address);
// The draws are forwarded with the eight argument registers as they came, so a wrapper does not depend on each draw's
// exact parameter list; the count of vertices or indices is the fourth argument of the four non-indirect draws.
using DrawFn = void (*)(void* cmd, u64, u64, u64, u64, u64, u64, u64);
using DrawArraysFn = DrawFn;

BindProgramFn g_bind_program;
BeginXfbFn g_begin_xfb;
EndXfbFn g_end_xfb;
enum DrawKind { kDrawArrays, kDrawElements, kDrawArraysInstanced, kDrawElementsInstanced, kDrawElementsIndirect, kDrawKinds };
DrawFn g_draws[kDrawKinds];
std::atomic<u64> g_xfb_draws_by_kind[kDrawKinds];

// The game records each draw into its own command buffer on one thread (FUN_00c4fa44), so the state of a command
// buffer is the program last bound and whether transform feedback is active. The table is direct-mapped: a command
// buffer takes over its slot, replacing whatever was there. The probe before this one kept the first 1,024 command
// buffers it saw and ignored every later one; the game uses one per draw, so it never counted a transform feedback
// draw (owner's log, 3 November: about 750 transform feedback passes a second, none counted). A draw follows its
// bind and Begin on the same command buffer, so a slot taken over in between only loses one probe count.
struct CommandState {
    void* cmd;
    const void* program;
    bool in_xfb;
};
constexpr size_t kStates = 4096;
CommandState g_states[kStates];

CommandState& SlotOf(void* cmd) { return g_states[(reinterpret_cast<uintptr_t>(cmd) >> 6) % kStates]; }

// Raw call counts, logged every 10 s by the watch thread, to show where the calls go even if the logic above is wrong.
std::atomic<u64> g_bind_calls, g_begin_calls, g_end_calls, g_draw_calls;

// Programs seen in transform feedback draws, with their draw and vertex counts.
struct ProgramStats {
    std::atomic<const void*> program;
    std::atomic<u64> draws;
    std::atomic<u64> vertices;
    std::atomic<u64> by_kind[kDrawKinds];
};
constexpr size_t kPrograms = 64;
ProgramStats g_programs[kPrograms];
std::atomic<u64> g_xfb_draws;

void CountXfbDraw(const void* program, int count, DrawKind kind) {
    g_xfb_draws_by_kind[kind].fetch_add(1, std::memory_order_relaxed);
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
        stats.by_kind[kind].fetch_add(1, std::memory_order_relaxed);
        break;
    }
    g_xfb_draws.fetch_add(1, std::memory_order_relaxed);
}

void BindProgram(void* cmd, const void* program, int stages) {
    g_bind_calls.fetch_add(1, std::memory_order_relaxed);
    CommandState& state = SlotOf(cmd);
    if (state.cmd != cmd) state = CommandState{cmd, nullptr, false};
    state.program = program;
    g_bind_program(cmd, program, stages);
}

void BeginXfb(void* cmd, u64 address) {
    g_begin_calls.fetch_add(1, std::memory_order_relaxed);
    CommandState& state = SlotOf(cmd);
    if (state.cmd != cmd) state = CommandState{cmd, nullptr, false};
    state.in_xfb = true;
    g_begin_xfb(cmd, address);
}

void EndXfb(void* cmd, u64 address) {
    g_end_calls.fetch_add(1, std::memory_order_relaxed);
    CommandState& state = SlotOf(cmd);
    if (state.cmd == cmd) state.in_xfb = false;
    g_end_xfb(cmd, address);
}

template <DrawKind kKind>
void Draw(void* cmd, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7) {
    g_draw_calls.fetch_add(1, std::memory_order_relaxed);
    if (const CommandState& state = SlotOf(cmd); state.cmd == cmd && state.in_xfb) {
        CountXfbDraw(state.program, kKind == kDrawElementsIndirect ? 0 : static_cast<int>(a3), kKind);
    }
    g_draws[kKind](cmd, a1, a2, a3, a4, a5, a6, a7);
}
constexpr DrawFn DrawArrays = &Draw<kDrawArrays>;

template <typename Fn>
void Swap(const scarlet::nvn_slots::Slot& where, Fn& original, Fn replacement) {
    Fn* slot = Slot<Fn>(where);
    if (slot == nullptr) return;
    Fn current = *slot;
    // a slot still holding the module's function is left as is: taking it for the game's would make the function
    // call itself
    if (current == nullptr || current == replacement) return;
    original = current;
    *slot = replacement;
}

void SwapAll(const char* when) {
    Swap(scarlet::nvn_slots::kCommandBufferBindProgram, g_bind_program, BindProgram);
    Swap(scarlet::nvn_slots::kCommandBufferBeginTransformFeedback, g_begin_xfb, BeginXfb);
    Swap(scarlet::nvn_slots::kCommandBufferEndTransformFeedback, g_end_xfb, EndXfb);
    Swap(scarlet::nvn_slots::kCommandBufferDrawArrays, g_draws[kDrawArrays], &Draw<kDrawArrays>);
    Swap(scarlet::nvn_slots::kCommandBufferDrawElements, g_draws[kDrawElements], &Draw<kDrawElements>);
    Swap(scarlet::nvn_slots::kCommandBufferDrawArraysInstanced, g_draws[kDrawArraysInstanced], &Draw<kDrawArraysInstanced>);
    Swap(scarlet::nvn_slots::kCommandBufferDrawElementsInstanced, g_draws[kDrawElementsInstanced], &Draw<kDrawElementsInstanced>);
    Swap(scarlet::nvn_slots::kCommandBufferDrawElementsIndirect, g_draws[kDrawElementsIndirect], &Draw<kDrawElementsIndirect>);
    Log("[scarlet_mali] 8 NVN calls intercepted %s (probe build, nothing drawn differently)", when);
}


// The pointers are swapped by a thread of the module once the game has filled them: the module changes no code of the
// game, only these data pointers. (A hook on the game's loader, FUN_00cdff20, never ran in the first probe: whether
// the loader runs after the module starts under Eden is not known.)
alignas(16) u8 g_watch_stack[0x4000];
u32 g_watch_ticks;

void WatchNvnSetup(void*) {
    // DrawArrays is among the last functions the loader fills; checked every 50 ms until it is, then every 500 ms in
    // case the loader runs again (it has two callers) and puts the game's own functions back: they are swapped again
    for (;;) {
        DrawArraysFn* slot = Slot<DrawArraysFn>(scarlet::nvn_slots::kCommandBufferDrawArrays);
        DrawArraysFn current = slot != nullptr ? *slot : nullptr;
        if (current != nullptr && current != DrawArrays) {
            svcSleepThread(50'000'000);  // the loader fills the rest of its 532 pointers in the meantime
            SwapAll(g_draws[kDrawArrays] == nullptr ? "once the game had set NVN up" : "again, after the game set NVN up again");
        }
        svcSleepThread(current == nullptr ? 50'000'000 : 500'000'000);
        if (current != nullptr && ++g_watch_ticks % 20 == 0) {
            Log("[scarlet_mali] calls so far: %llu BindProgram, %llu BeginTransformFeedback, %llu EndTransformFeedback, "
                "%llu draws, %llu draws inside transform feedback",
                static_cast<unsigned long long>(g_bind_calls.load()), static_cast<unsigned long long>(g_begin_calls.load()),
                static_cast<unsigned long long>(g_end_calls.load()), static_cast<unsigned long long>(g_draw_calls.load()),
                static_cast<unsigned long long>(g_xfb_draws.load()));
            // the game records each draw once and replays it (the counts above stay in the thousands while Eden sees
            // hundreds of transform feedback passes a second), so the details are logged here rather than every
            // 5,000 transform feedback draws, a count the previous probe never reached
            Log("[scarlet_mali]   inside transform feedback by call: %llu DrawArrays, %llu DrawElements, %llu DrawArraysInstanced, "
                "%llu DrawElementsInstanced, %llu DrawElementsIndirect",
                static_cast<unsigned long long>(g_xfb_draws_by_kind[0].load()), static_cast<unsigned long long>(g_xfb_draws_by_kind[1].load()),
                static_cast<unsigned long long>(g_xfb_draws_by_kind[2].load()), static_cast<unsigned long long>(g_xfb_draws_by_kind[3].load()),
                static_cast<unsigned long long>(g_xfb_draws_by_kind[4].load()));
            for (auto& stats : g_programs) {
                const void* program_seen = stats.program.load(std::memory_order_acquire);
                if (program_seen == nullptr) break;
                Log("[scarlet_mali]   program %p: %llu draws, %llu vertices or indices", program_seen,
                    static_cast<unsigned long long>(stats.draws.load()), static_cast<unsigned long long>(stats.vertices.load()));
            }
        }
    }
}

}  // namespace

extern "C" void exl_main(void* x0, void* x1) {
    Log("[scarlet_mali] module loaded (main at %p)", reinterpret_cast<void*>(MainBase()));
    if (Slot<DrawArraysFn>(scarlet::nvn_slots::kCommandBufferDrawArrays) == nullptr) {
        Log("[scarlet_mali] main's GOT does not match this build's offsets: nothing intercepted");
        return;
    }
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
