// SPDX-FileCopyrightText: Copyright Pomegrade Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: TEMPORARY diagnostic counters for the optimisation study (docs/Eden_Optimisation_Xiaomi.md, section 4,
// no ADB). Read once a second by the sampler in android/app/src/main/jni/native.cpp, which writes them to Eden's
// log. To be deleted with every use of it once the owner's log is read (CLAUDE.md §2.4).

#pragma once

#include <atomic>
#include <cstdint>

namespace PomegradeDiagnostics {

// Scheduler::Finish calls (a CPU thread waiting for the GPU to have run everything submitted)
inline std::atomic<std::uint64_t> finish_calls{0};
// MasterSemaphore::Wait calls that had to wait, and the time spent waiting, in nanoseconds
inline std::atomic<std::uint64_t> gpu_waits{0};
inline std::atomic<std::uint64_t> gpu_wait_ns{0};
// guest memory accesses caught by NCE's fault handler (memory tracking)
inline std::atomic<std::uint64_t> nce_access_faults{0};

// the same waits split by the waiting thread (its name): which part of the emulator stops for the GPU
enum WaitThread : int { WaitGpu, WaitFencing, WaitPresent, WaitWorker, WaitGuest, WaitOther, WaitThreadCount };
inline constexpr const char* wait_thread_names[WaitThreadCount] = {"GPU", "fencing", "present", "worker", "guest CPU", "other"};
// the GPU thread blocked in ReleaseFences (a guest semaphore or syncpoint wait): until the fencing thread has
// released every pending fence, i.e. until the GPU has run everything submitted
inline std::atomic<std::uint64_t> release_waits{0};
inline std::atomic<std::uint64_t> release_wait_ns{0};
inline std::atomic<std::uint64_t> waits_by_thread[WaitThreadCount]{};
inline std::atomic<std::uint64_t> wait_ns_by_thread[WaitThreadCount]{};
// frames the game finished (PerfStats::EndGameFrame), and the longest gap between two of them, in nanoseconds
inline std::atomic<std::uint64_t> game_frames{0};
inline std::atomic<std::uint64_t> longest_frame_ns{0};
inline std::atomic<std::int64_t> last_frame_ns{0};

// render passes begun, and their area summed (pixels): passes per frame and how many screens of pixels they cover,
// each one loaded from memory and stored back (LOAD_OP_LOAD, STORE_OP_STORE)
inline std::atomic<std::uint64_t> render_passes{0};
inline std::atomic<std::uint64_t> render_pass_pixels{0};
// the current pass has had no draw nor clear yet (set when a pass begins)
inline std::atomic<bool> pass_untouched{false};
// clears covering the whole render area: as the pass's first command (its load could be a CLEAR, not a LOAD) or later
inline std::atomic<std::uint64_t> full_clears_first{0};
inline std::atomic<std::uint64_t> full_clears_later{0};
// full clears turned into their pass's load (LOAD_OP_CLEAR, setting pomegrade_clear_on_load), per attachment
inline std::atomic<std::uint64_t> clears_on_load{0};
// images made: renderable ones, renderable ones also given STORAGE usage (no AFBC compression on Mali), and storage
// views made on renderable images (the game really using one as storage)
inline std::atomic<std::uint64_t> attachment_images{0};
inline std::atomic<std::uint64_t> storage_attachment_images{0};
inline std::atomic<std::uint64_t> attachment_storage_views{0};

} // namespace PomegradeDiagnostics
