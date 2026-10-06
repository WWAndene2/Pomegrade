// Copyright Pomegrade
// Licensed under GPLv2 or any later version

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"
#include "core/pomegrade_code_trace.h"

namespace Pomegrade::CodeTrace {

namespace {

// how much is kept: the last events of each kind (about the last minutes before a freeze)
constexpr std::size_t MaxCalls = 1 << 17;     // kernel calls and service requests
constexpr std::size_t MaxNotable = 20000;     // files and modules
constexpr std::size_t MaxSnapshots = 2400;    // 10 minutes at 4 a second
constexpr auto SnapshotEvery = std::chrono::milliseconds(250);
constexpr auto WriteEvery = std::chrono::seconds(30);

struct Call {
    u64 time_us;
    u32 thread, pc, lr;
    u32 words[5]; // a service request's first command words
    u8 count;     // how many of words are set
    bool kernel;
    char name[46];
};

struct Module {
    std::string name;
    u32 address, end;
};

std::mutex mutex; // the record, shared with the writing thread
std::atomic<bool> configured{false};
std::atomic<bool> active{false};
std::atomic<bool> writing{false};
std::string base_folder;
std::string folder; // this game's record
u64 program;

std::vector<Call> calls; // a ring of MaxCalls
std::size_t next_call = 0;
u64 total_calls = 0;
std::deque<std::string> notable;
std::deque<std::string> snapshots;
std::vector<Module> modules;
std::chrono::steady_clock::time_point last_snapshot, last_write;

std::string Stamp(u64 time_us) {
    char text[32];
    std::snprintf(text, sizeof text, "%10.3f s", time_us / 1e6);
    return text;
}

u64 Now(Core::System& system) {
    return static_cast<u64>(system.CoreTiming().GetGlobalTimeUs().count());
}

// the game's process is the one running the call (system modules are left out)
bool InGame(Core::System& system) {
    const auto process = system.Kernel().GetCurrentProcess();
    return process && process->codeset && process->codeset->program_id == program;
}

u32 CurrentThread(Core::System& system) {
    const auto* thread = system.Kernel().GetCurrentThreadManager().GetCurrentThread();
    return thread ? thread->thread_id : 0;
}

void Push(Call&& call) {
    if (calls.size() < MaxCalls) {
        calls.push_back(std::move(call));
    } else {
        calls[next_call] = std::move(call);
    }
    next_call = (next_call + 1) % MaxCalls;
    total_calls++;
}

void PushLimited(std::deque<std::string>& list, std::string&& line, std::size_t max) {
    list.push_back(std::move(line));
    if (list.size() > max) {
        list.pop_front();
    }
}

// a word that is an address in the game's code or a module it loaded
bool IsCode(const Kernel::Process& process, u32 word) {
    const auto& code = process.codeset->CodeSegment();
    if (word >= code.addr && word < code.addr + code.size) {
        return true;
    }
    return std::any_of(modules.begin(), modules.end(),
                       [word](const Module& m) { return word >= m.address && word < m.end; });
}

std::string Snapshot(Core::System& system) {
    static const char* const states[] = {"running",        "ready",          "wait-arbiter",
                                         "wait-sleep",     "wait-ipc",       "wait-synch-any",
                                         "wait-synch-all", "wait-hle",       "dormant",
                                         "dead"};
    auto& kernel = system.Kernel();
    auto& memory = system.Memory();
    std::string out = "== " + Stamp(Now(system)) + "\n";
    char line[256];
    for (u32 core = 0; core < system.GetNumCores(); core++) {
        const auto& manager = kernel.GetThreadManager(core);
        const auto* current = manager.GetCurrentThread();
        for (const auto& thread : manager.GetThreadList()) {
            const auto process = thread->owner_process.lock();
            if (!process || !process->codeset || process->codeset->program_id != program) {
                continue;
            }
            const bool live = thread.get() == current;
            auto& cpu = system.GetCore(core);
            const u32 pc = live ? cpu.GetPC() : thread->context.GetProgramCounter();
            const u32 lr = live ? cpu.GetReg(14) : thread->context.GetLinkRegister();
            const u32 sp = live ? cpu.GetReg(13) : thread->context.GetStackPointer();
            const int status = static_cast<int>(thread->status);
            std::snprintf(line, sizeof line,
                          "thread %u core %u %s%s pc %08X lr %08X sp %08X waits on %zu\n",
                          thread->thread_id, core,
                          status >= 0 && status < 10 ? states[status] : "?",
                          live ? " (on the core)" : "", pc, lr, sp, thread->wait_objects.size());
            out += line;
            out += "  stack:";
            int found = 0;
            for (u32 at = sp; at < sp + 0x800 && found < 24; at += 4) {
                if (!memory.IsValidVirtualAddress(*process, at)) {
                    break;
                }
                const u32 word = memory.Read32(*process, at);
                if (IsCode(*process, word)) {
                    std::snprintf(line, sizeof line, " %08X@+%X", word, at - sp);
                    out += line;
                    found++;
                }
            }
            out += "\n";
        }
    }
    return out;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary | std::ios::trunc).write(text.data(), text.size());
}

// the record as text, in the game's folder: one file per kind, oldest first
void Write(std::string dir, std::vector<Call> ring, std::size_t next, u64 total,
           std::deque<std::string> notes, std::deque<std::string> shots, u64 program_id) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return;
    }
    std::string text;
    text.reserve(ring.size() * 80);
    char line[256];
    const std::size_t count = ring.size();
    for (std::size_t i = 0; i < count; i++) {
        const Call& c = ring[count < MaxCalls ? i : (next + i) % count];
        std::snprintf(line, sizeof line, "%s thread %u pc %08X lr %08X %s %s", Stamp(c.time_us).c_str(),
                      c.thread, c.pc, c.lr, c.kernel ? "svc" : "ipc", c.name);
        text += line;
        for (u8 w = 0; w < c.count; w++) {
            std::snprintf(line, sizeof line, " %08X", c.words[w]);
            text += line;
        }
        text += "\n";
    }
    WriteFile(std::filesystem::path(dir) / "calls.txt", text);

    text.clear();
    for (const auto& n : notes) {
        text += n;
    }
    WriteFile(std::filesystem::path(dir) / "files_and_modules.txt", text);

    text.clear();
    for (const auto& s : shots) {
        text += s;
    }
    WriteFile(std::filesystem::path(dir) / "threads.txt", text);

    std::snprintf(line, sizeof line,
                  "Pomegrade code trace\nprogram %016llX\nkernel calls and service requests: %llu, "
                  "the last %zu kept (calls.txt)\nthread snapshots kept: %zu, every %lld ms "
                  "(threads.txt)\n",
                  static_cast<unsigned long long>(program_id), static_cast<unsigned long long>(total),
                  count, shots.size(), static_cast<long long>(SnapshotEvery.count()));
    WriteFile(std::filesystem::path(dir) / "summary.txt", line);
}

// copies the record (under the lock) and writes it; in the background unless wait
void WriteRecord(bool wait) {
    bool expected = false;
    if (!writing.compare_exchange_strong(expected, true)) {
        if (!wait) {
            return;
        }
        while (writing.exchange(true)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    std::unique_lock lock{mutex};
    auto job = [dir = folder, ring = calls, next = next_call, total = total_calls, notes = notable,
                shots = snapshots, id = program]() mutable {
        Write(std::move(dir), std::move(ring), next, total, std::move(notes), std::move(shots), id);
        writing = false;
    };
    lock.unlock();
    if (wait) {
        job();
    } else {
        std::thread(std::move(job)).detach();
    }
}

} // namespace

void Configure(bool enabled, const std::string& where) {
    std::scoped_lock lock{mutex};
    configured = enabled && !where.empty();
    base_folder = where;
}

bool Enabled() {
    return active;
}

void Start(Core::System& system, u64 program_id) {
    if (!configured) {
        active = false;
        return;
    }
    std::scoped_lock lock{mutex};
    program = program_id;
    char name[64];
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char date[32];
    std::strftime(date, sizeof date, "%Y-%m-%d_%H-%M-%S", &local);
    std::snprintf(name, sizeof name, "%016llX_%s", static_cast<unsigned long long>(program_id), date);
    folder = (std::filesystem::path(base_folder) / name).string();
    calls.clear();
    calls.reserve(MaxCalls);
    next_call = 0;
    total_calls = 0;
    notable.clear();
    snapshots.clear();
    modules.clear();
    last_snapshot = last_write = std::chrono::steady_clock::now();
    active = true;
}

void Stop() {
    if (!active) {
        return;
    }
    active = false;
    WriteRecord(true);
}

void KernelCall(Core::System& system, u32 id, const char* name) {
    if (!active || !InGame(system)) {
        return;
    }
    auto& cpu = system.GetRunningCore();
    Call call{Now(system), CurrentThread(system), cpu.GetPC(), cpu.GetReg(14), {}, 0, true, {}};
    // the call's first arguments (handles, timeouts, addresses)
    for (u8 r = 0; r < 4; r++) {
        call.words[r] = cpu.GetReg(r);
    }
    call.count = 4;
    std::snprintf(call.name, sizeof call.name, "%02X %s", id, name ? name : "?");
    std::scoped_lock lock{mutex};
    Push(std::move(call));
}

void ServiceRequest(Core::System& system, const std::string& service, const char* request,
                    const u32* command, std::size_t words) {
    if (!active || !InGame(system)) {
        return;
    }
    auto& cpu = system.GetRunningCore();
    Call call{Now(system), CurrentThread(system), cpu.GetPC(), cpu.GetReg(14), {}, 0, false, {}};
    call.count = static_cast<u8>(std::min<std::size_t>(words, 5));
    for (u8 w = 0; w < call.count; w++) {
        call.words[w] = command[w];
    }
    std::snprintf(call.name, sizeof call.name, "%s:%s", service.c_str(), request ? request : "?");
    std::scoped_lock lock{mutex};
    Push(std::move(call));
}

void FileOpened(Core::System& system, const char* what, const std::string& path) {
    if (!active || !InGame(system)) {
        return;
    }
    auto& cpu = system.GetRunningCore();
    char line[96];
    std::snprintf(line, sizeof line, " thread %u lr %08X %s ", CurrentThread(system), cpu.GetReg(14), what);
    std::scoped_lock lock{mutex};
    PushLimited(notable, Stamp(Now(system)) + line + path + "\n", MaxNotable);
}

void ModuleLoaded(Core::System& system, const std::string& name, u32 address, u32 end) {
    if (!active) {
        return;
    }
    char line[96];
    std::snprintf(line, sizeof line, " module %08X-%08X ", address, end);
    std::scoped_lock lock{mutex};
    modules.push_back({name, address, end});
    PushLimited(notable, Stamp(Now(system)) + line + name + "\n", MaxNotable);
}

void Tick(Core::System& system) {
    if (!active) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - last_snapshot >= SnapshotEvery) {
        last_snapshot = now;
        std::string shot;
        {
            std::scoped_lock lock{mutex};
            shot = Snapshot(system);
            PushLimited(snapshots, std::move(shot), MaxSnapshots);
        }
    }
    if (now - last_write >= WriteEvery) {
        last_write = now;
        WriteRecord(false);
    }
}

} // namespace Pomegrade::CodeTrace
