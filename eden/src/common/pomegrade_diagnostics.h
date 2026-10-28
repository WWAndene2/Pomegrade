// SPDX-FileCopyrightText: Copyright Pomegrade Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: TEMPORARY diagnostic counters for the optimisation study (docs/Eden_Optimisation_Xiaomi.md, section 4,
// no ADB). Read once a second by the sampler in android/app/src/main/jni/native.cpp, which writes them to Eden's
// log. To be deleted with every use of it once the owner's log is read (CLAUDE.md §2.4).

#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

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
// the guest's "wait for idle" made at the end of the render pass instead of ending it (pomegrade_defer_wait_for_idle)
inline std::atomic<std::uint64_t> deferred_waits{0};
// images made: renderable ones, renderable ones also given STORAGE usage (no AFBC compression on Mali), and storage
// views made on renderable images (the game really using one as storage)
inline std::atomic<std::uint64_t> attachment_images{0};
inline std::atomic<std::uint64_t> storage_attachment_images{0};
inline std::atomic<std::uint64_t> attachment_storage_views{0};

// why render passes end: the code that closed each one (file:line, or "new render target" when the next pass needs
// other attachments, "submit" when the command buffer is sent), counted
inline std::mutex pass_end_mutex;
inline std::map<std::string, std::uint64_t> pass_ends;
inline void CountPassEnd(const char* file, int line) {
    std::string where = file;
    const auto slash = where.rfind('/');
    if (slash != std::string::npos) {
        where = where.substr(slash + 1);
    }
    if (line > 0) {
        where += ":" + std::to_string(line);
    }
    std::scoped_lock lock{pass_end_mutex};
    ++pass_ends[where];
}

// transform feedback draws (GraphicsPipeline::ConfigureImpl ends the pass before each one): whether they draw
// anything (rasterizer discard, colour targets, fragment stage), whether they use a geometry shader, how many runs
// of back-to-back ones (what batching them would save), and the later draws reading their output as vertices
inline std::atomic<std::uint64_t> tf_draws{0};
inline std::atomic<std::uint64_t> tf_draws_discard{0};
inline std::atomic<std::uint64_t> tf_draws_to_color{0};
inline std::atomic<std::uint64_t> tf_draws_fragment{0};
inline std::atomic<std::uint64_t> tf_draws_geometry{0};
inline std::atomic<std::uint64_t> tf_runs{0};
inline std::atomic<std::uint64_t> tf_consumer_draws{0};
inline std::atomic<std::uint64_t> tf_consumers_next_draw{0};
inline std::mutex tf_mutex;
inline std::vector<std::pair<std::uint64_t, std::uint64_t>> tf_outputs; // [begin, end) GPU addresses, this second
inline bool tf_last_draw{false};
inline void CountTfDraw(bool discard, bool to_color, bool fragment, bool geometry,
                        const std::pair<std::uint64_t, std::uint64_t>* outputs, int output_count) {
    tf_draws.fetch_add(1, std::memory_order_relaxed);
    tf_draws_discard.fetch_add(discard, std::memory_order_relaxed);
    tf_draws_to_color.fetch_add(to_color, std::memory_order_relaxed);
    tf_draws_fragment.fetch_add(fragment, std::memory_order_relaxed);
    tf_draws_geometry.fetch_add(geometry, std::memory_order_relaxed);
    std::scoped_lock lock{tf_mutex};
    tf_runs.fetch_add(!tf_last_draw, std::memory_order_relaxed);
    tf_last_draw = true;
    for (int i = 0; i < output_count && tf_outputs.size() < 4096; i++) {
        tf_outputs.push_back(outputs[i]);
    }
}
// a draw without transform feedback: does one of its vertex streams start inside a range written by one
inline void CountNonTfDraw(const std::uint64_t* stream_addresses, int stream_count) {
    std::scoped_lock lock{tf_mutex};
    const bool after_tf = tf_last_draw;
    tf_last_draw = false;
    for (int i = 0; i < stream_count; i++) {
        for (const auto& [begin, end] : tf_outputs) {
            if (stream_addresses[i] >= begin && stream_addresses[i] < end) {
                tf_consumer_draws.fetch_add(1, std::memory_order_relaxed);
                tf_consumers_next_draw.fetch_add(after_tf, std::memory_order_relaxed);
                return;
            }
        }
    }
}

} // namespace PomegradeDiagnostics
