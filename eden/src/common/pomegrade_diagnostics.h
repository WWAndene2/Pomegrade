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

#ifdef __ANDROID__
#include <dlfcn.h>
#endif

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
// clears covering the whole render area: deferred by Eden into the next pass's CLEAR load (no pass open), as the
// open pass's first command (its load could be a CLEAR, not a LOAD) or later
inline std::atomic<std::uint64_t> full_clears_deferred{0};
inline std::atomic<std::uint64_t> full_clears_first{0};
inline std::atomic<std::uint64_t> full_clears_later{0};
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

// the shape of the transform feedback draws (O13, docs/Eden_Optimisation_Xiaomi.md: where a vertex shader would write
// each captured vertex depends on it): topology, indexed or not, instances, and each capture buffer's stride and
// varyings; the vertices (or indices) they send; the indirect ones (their counts unknown on the CPU)
inline std::mutex tf_shape_mutex;
inline std::map<std::string, std::uint64_t> tf_shapes; // shape -> draws, this second
inline std::atomic<std::uint64_t> tf_vertices{0};
inline std::atomic<std::uint64_t> tf_indirect_draws{0};
inline void CountTfShape(const std::string& shape, std::uint64_t vertices) {
    tf_vertices.fetch_add(vertices, std::memory_order_relaxed);
    std::scoped_lock lock{tf_shape_mutex};
    if (tf_shapes.size() < 64 || tf_shapes.contains(shape)) {
        tf_shapes[shape]++;
    }
}

// the code that waited for the GPU (Scheduler::Wait's caller, file:line), per waiting thread, counted with the time
inline thread_local const char* wait_file = nullptr;
inline thread_local int wait_line = 0;
inline std::mutex wait_site_mutex;
inline std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> wait_sites; // count, ns
inline void CountWaitSite(const char* thread, std::uint64_t ns) {
    std::string where = wait_file ? wait_file : "?";
    const auto slash = where.rfind('/');
    if (slash != std::string::npos) {
        where = where.substr(slash + 1);
    }
    where = std::string(thread) + " " + where + ":" + std::to_string(wait_line);
    std::scoped_lock lock{wait_site_mutex};
    auto& [count, total] = wait_sites[where];
    ++count;
    total += ns;
}

// GPU time measured with Vulkan timestamps (Scheduler): command buffers from start to end, the runs of transform
// feedback draws, and the pairs dropped because no query was free
inline std::atomic<std::uint64_t> gpu_cmdbuf_ns{0};
inline std::atomic<std::uint64_t> gpu_cmdbufs{0};
inline std::atomic<std::uint64_t> gpu_tf_ns{0};
// GPU time of each render pass (Vulkan timestamps around it), summed by the pass's kind: render area, attachment
// count and draws in it (draws counted between its begin and end): where the frame's GPU time goes
inline std::atomic<std::uint64_t> pass_draws{0};
inline std::mutex pass_time_mutex;
inline std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> pass_time; // kind -> {passes, ns}
inline std::atomic<std::uint64_t> gpu_tf_runs{0};
inline std::atomic<std::uint64_t> gpu_timer_dropped{0};
inline std::atomic<bool> gpu_timer_available{false};
inline bool tf_run_open{false}; // the GPU thread's current run has a timestamp pair (GraphicsPipeline)
inline std::uint32_t tf_run_pair{0};

// Perfetto/systrace sections (ATrace of libandroid.so, which the app links, looked up once): visible in a system trace of the app
inline void TraceBegin(const char* name) {
#ifdef __ANDROID__
    static auto begin = reinterpret_cast<void (*)(const char*)>(dlsym(RTLD_DEFAULT, "ATrace_beginSection"));
    if (begin) {
        begin(name);
    }
#else
    (void)name;
#endif
}
inline void TraceEnd() {
#ifdef __ANDROID__
    static auto end = reinterpret_cast<void (*)()>(dlsym(RTLD_DEFAULT, "ATrace_endSection"));
    if (end) {
        end();
    }
#endif
}
struct TraceScope {
    explicit TraceScope(const char* name) {
        TraceBegin(name);
    }
    ~TraceScope() {
        TraceEnd();
    }
    TraceScope(const TraceScope&) = delete;
    TraceScope& operator=(const TraceScope&) = delete;
};

} // namespace PomegradeDiagnostics
