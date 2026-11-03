// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <deque>
#include <string>
#include <chrono>
#include <mutex>
#include <map>
#include <thread>
#include <utility>
#include <queue>

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "common/settings.h"
#include "video_core/gpu_split.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace VideoCommon {
template <typename Trait>
class QueryCacheBase;
}

namespace Vulkan {

class CommandPool;
class Device;
class Framebuffer;
class GraphicsPipeline;
class StateTracker;

struct QueryCacheParams;

/// The scheduler abstracts command buffer and fence management with an interface that's able to do
/// OpenGL-like operations on Vulkan command buffers.
class Scheduler {
public:
    explicit Scheduler(const Device& device, StateTracker& state_tracker);
    ~Scheduler();

    /// Sends the current execution context to the GPU.
    u64 Flush(VkSemaphore signal_semaphore = {}, VkSemaphore wait_semaphore = {});

    /// Sends the current execution context to the GPU and waits for it to complete.
    void Finish(VkSemaphore signal_semaphore = {}, VkSemaphore wait_semaphore = {});

    /// Waits for the worker thread to finish executing everything. After this function returns it's
    /// safe to touch worker resources.
    void WaitWorker();

    /// Sends currently recorded work to the worker thread.
    void DispatchWork();

    /// Requests to begin a renderpass.
    void RequestRenderpass(const Framebuffer* framebuffer);

    /// Defers a full-attachment color clear so it becomes the next render pass.
    bool DeferColorClear(const Framebuffer* framebuffer, u32 rt_slot, const VkClearValue& value);

    /// Defers a full depth/stencil clear so it becomes the next render pass.
    bool DeferDepthStencilClear(const Framebuffer* framebuffer, const VkClearValue& value);

    /// Realizes any pending deferred clear before its framebuffer can be moved or freed.
    void FlushDeferredClear();

    /// Pomegrade: the framebuffer with no attachment, for draws that rasterize nothing (set by the rasterizer)
    void SetNullFramebuffer(const Framebuffer* framebuffer) noexcept {
        null_framebuffer = framebuffer;
    }

    [[nodiscard]] const Framebuffer* NullFramebuffer() const noexcept {
        return null_framebuffer;
    }

    /// Pomegrade: TEMPORARY, a draw recorded in the open render pass (the per-pass GPU time's draw count)
    void PomegradeCountDraw() noexcept {
        ++pomegrade_pass_draws;
    }

    /// Pomegrade: a "wait for idle" made at the end of the current render pass (a full memory barrier after it)
    /// instead of ending it now (setting pomegrade_defer_wait_for_idle)
    void DeferWaitForIdle() noexcept {
        deferred_wait_for_idle = true;
    }

    /// Requests the current execution context to be able to execute operations only allowed outside
    /// of a renderpass.
    void RequestOutsideRenderPassOperationContext();

    /// Pomegrade: makes the vertex, geometry, compute and indirect stages of the work recorded next wait for
    /// the fragment work of the render passes ended so far (see EndRenderPass); ends the open pass first
    /// when one ended before it is still unwaited for
    void RequestNonFragmentDependency();

    /// Pomegrade: the open render pass draws into [framebuffer]
    [[nodiscard]] bool IsRenderPassOn(const Framebuffer* framebuffer) const noexcept;

    /// Pomegrade: the open render pass holds a transform feedback draw, so its end makes the captured
    /// data visible to vertex input and transfers (EndRenderPass)
    void MarkTransformFeedbackInPass() noexcept {
        pass_has_transform_feedback = true;
    }

    /// Pomegrade: capture hoisting (setting pomegrade_hoist_captures). A transform feedback capture that rasterizes
    /// nothing runs in a pass without attachments (GraphicsPipeline, uses_null_render_pass), so before this each
    /// one made while the game's pass was open ended that pass and began it again after: on a tiler (Mali) the
    /// pass's attachments are written out of tile memory and read back in at each, and Pokémon Scarlet makes such
    /// a capture before each animated model it draws. Instead the capture is recorded on its own (BeginIsolation,
    /// EndIsolation) and placed before the open pass's begin, in the space each eligible pass keeps in front of
    /// it (its prologue), so the pass stays whole.
    ///
    /// Moving the capture before the pass's earlier draws is only right when those draws did not read what it
    /// writes (TF output ranges against the vertex, index and uniform ranges they read, NoteMainPassRead) nor
    /// write memory it may read (MarkPassWritesMemory); textures and storage buffers reading a capture's output
    /// are not tracked, which is the known limit, hence the setting. A capture that needs anything recorded
    /// outside a pass (an upload, a copy, a query write) is not moved: it is placed where it was made, after
    /// the open pass is ended, as before.
    [[nodiscard]] bool CanHoistIntoPass() const noexcept;
    void BeginIsolation();
    void EndIsolation();
    [[nodiscard]] bool IsIsolating() const noexcept {
        return isolating;
    }
    /// Pomegrade: TEMPORARY, why a capture was not hoisted (index into the log's reasons, NoteHoistRefusal)
    void NoteHoistRefusal(size_t reason) noexcept {
        if (reason < pomegrade_hoist_refusals.size()) {
            ++pomegrade_hoist_refusals[reason];
        }
    }
    /// Pomegrade: TEMPORARY, which of CanHoistIntoPass's conditions fails first (0 when none)
    [[nodiscard]] size_t HoistIntoPassRefusal() const noexcept;
    /// Pomegrade: the open pass's draws' reads are wanted (capture hoisting)
    [[nodiscard]] bool WantsPassReads() const noexcept {
        return pass_prologue != nullptr && !isolating;
    }
    /// Pomegrade: a draw of the open pass reads guest memory [begin, end) (capture hoisting)
    void NoteMainPassRead(u64 begin, u64 end);
    [[nodiscard]] bool MainPassReadOverlaps(u64 begin, u64 end) const noexcept;
    /// Pomegrade: a draw of the open pass writes memory through a shader (capture hoisting)
    void MarkPassWritesMemory() noexcept {
        if (!isolating) {
            main_pass_writes_memory = true;
        }
    }

    /// Returns true when a render pass is currently active in the scheduler state.
    bool IsRenderPassActive() const {
        return state.renderpass != VK_NULL_HANDLE;
    }

    /// Update the pipeline to the current execution context.
    bool UpdateGraphicsPipeline(GraphicsPipeline* pipeline);

    /// Update the rescaling state. Returns true if the state has to be updated.
    bool UpdateRescaling(bool is_rescaling);

    /// Returns true when the descriptor buffer chunk has to be bound into the command buffer.
    bool UpdateDescriptorBufferChunk(u32 descriptor_chunk);

    /// Invalidates current command buffer state except for render passes
    void InvalidateState();

    /// Assigns the query cache.
    void SetQueryCache(VideoCommon::QueryCacheBase<QueryCacheParams>& query_cache_) {
        query_cache = &query_cache_;
    }

    // Registers a callback to perform on queue submission.
    void RegisterOnSubmit(std::function<void()>&& func) {
        on_submit = std::move(func);
    }

    /// Send work to a separate thread.
    template <typename T>
        requires std::is_invocable_v<T, vk::CommandBuffer, vk::CommandBuffer>
    void RecordWithUploadBuffer(T&& command) {
        if (VideoCommon::GpuSplit::check_owner) [[unlikely]] {
            VideoCommon::GpuSplit::CheckOwner("Scheduler::Record");
        }
        if (isolating) [[unlikely]] {
            isolated_commands->Record(command);
            return;
        }
        if (chunk->Record(command)) {
            return;
        }
        DispatchWork();
        (void)chunk->Record(command);
    }

    template <typename T>
        requires std::is_invocable_v<T, vk::CommandBuffer>
    void Record(T&& c) {
        this->RecordWithUploadBuffer(
            [command = std::move(c)](vk::CommandBuffer cmdbuf, vk::CommandBuffer) {
                command(cmdbuf);
            });
    }

    /// Whether the recording thread submitted `tick` within 20 ms.
    [[nodiscard]] bool WaitForSubmission(u64 tick) const {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{20};
        while (tick >= master_semaphore->CurrentTick()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::microseconds{100});
        }
        return true;
    }

    /// Returns the current command buffer tick.
    [[nodiscard]] u64 CurrentTick() const noexcept {
        return master_semaphore->CurrentTick();
    }

    /// Returns true when a tick has been triggered by the GPU.
    [[nodiscard]] bool IsFree(u64 tick) const noexcept {
        return master_semaphore->IsFree(tick);
    }

    /// Waits for the given GPU tick, optionally pacing frames. Threads other than the one that
    /// records (the async present thread) pass may_flush = false: flushing from there records
    /// into the current chunk while that thread does (corrupt command buffers, crashes). They
    /// wait for the recording thread to submit the tick instead, and give up after 20 ms (it may
    /// be waiting for them).
    void Wait(u64 tick, double target_fps = 0.0, bool may_flush = true) {
        if (tick > 0 && (may_flush || WaitForSubmission(tick))) {
            if (tick >= master_semaphore->CurrentTick()) {
                Flush();
            }
            master_semaphore->Wait(tick);
        }
        if (Settings::values.use_speed_limit.GetValue() && target_fps > 0.0) {
            auto now = std::chrono::steady_clock::now();
            if (last_target_fps != target_fps) {
                frame_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / target_fps));
                max_frame_count = static_cast<int>(0.1 * target_fps);
                last_target_fps = target_fps;
                frame_counter = 0;
                start_time = now;
            }
            frame_counter++;
            auto target_time = start_time + frame_interval * frame_counter;
            if (target_time >= now) {
                constexpr auto spin_tail = std::chrono::milliseconds(1);
                auto sleep_time = target_time - now;
                if (sleep_time > spin_tail * 2) {
                    std::this_thread::sleep_for(sleep_time - spin_tail);
                }
                while (std::chrono::steady_clock::now() < target_time) {
                    std::this_thread::yield();
                }
            } else if (frame_counter > max_frame_count) {
                frame_counter = 0;
                start_time = now;
            }
        }
    }

    /// Returns the master timeline semaphore.
    [[nodiscard]] MasterSemaphore& GetMasterSemaphore() const noexcept {
        return *master_semaphore;
    }

    std::mutex submit_mutex;

private:
    class Command {
    public:
        virtual ~Command() = default;

        virtual void Execute(vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) const = 0;

        Command* GetNext() const {
            return next;
        }

        void SetNext(Command* next_) {
            next = next_;
        }

    private:
        Command* next = nullptr;
    };

    template <typename T>
    class TypedCommand final : public Command {
    public:
        explicit TypedCommand(T&& command_) : command{std::move(command_)} {}
        ~TypedCommand() override = default;

        TypedCommand(TypedCommand&&) = delete;
        TypedCommand& operator=(TypedCommand&&) = delete;

        void Execute(vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) const override {

            command(cmdbuf, upload_cmdbuf);
        }

    private:
        T command;
    };

    class CommandChunk final {
    public:
        void ExecuteAll(vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf);

        template <typename T>
        bool Record(T& command) {
            using FuncType = TypedCommand<T>;
            static_assert(sizeof(FuncType) < sizeof(data), "Lambda is too large");

            command_offset = Common::AlignUp(command_offset, alignof(FuncType));
            if (command_offset > sizeof(data) - sizeof(FuncType)) {
                return false;
            }
            Command* const current_last = last;
            last = new (data.data() + command_offset) FuncType(std::move(command));

            if (current_last) {
                current_last->SetNext(last);
            } else {
                first = last;
            }
            command_offset += sizeof(FuncType);
            return true;
        }

        void MarkSubmit() {
            submit = true;
        }

        bool Empty() const {
            return command_offset == 0;
        }

        bool HasSubmit() const {
            return submit;
        }

    private:
        Command* first = nullptr;
        Command* last = nullptr;

        size_t command_offset = 0;
        bool submit = false;
        alignas(std::max_align_t) std::array<u8, 0x8000> data{};
    };

    /// Pomegrade: commands recorded apart from the chunk (an isolated capture, a pass's prologue), executed in
    /// order where their Execute is recorded
    class CommandList {
    public:
        template <typename T>
        void Record(T& command) {
            if (chunks.empty() || !chunks.back()->Record(command)) {
                chunks.push_back(std::make_unique<CommandChunk>());
                (void)chunks.back()->Record(command);
            }
        }
        void Append(CommandList&& other) {
            for (auto& other_chunk : other.chunks) {
                chunks.push_back(std::move(other_chunk));
            }
            other.chunks.clear();
        }
        void Execute(vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
            for (auto& list_chunk : chunks) {
                list_chunk->ExecuteAll(cmdbuf, upload_cmdbuf);
            }
        }

    private:
        std::vector<std::unique_ptr<CommandChunk>> chunks;
    };

    struct State {
        VkRenderPass renderpass{};
        VkFramebuffer framebuffer{};
        VkExtent2D render_area = {0, 0};
        GraphicsPipeline* graphics_pipeline = nullptr;
        bool is_rescaling = false;
        bool rescaling_defined = false;
        bool needs_state_enable_refresh = false;
        u32 descriptor_buffer_chunk = 0;
        bool descriptor_buffer_bound = false;
    };

    struct DeferredClear {
        const Framebuffer* framebuffer = nullptr;
        u32 color_clear_mask = 0;
        std::array<VkClearValue, 8> color_values{};
        bool depth_stencil = false;
        VkClearValue depth_stencil_value{};
    };

    /// Pomegrade: what the recording follows of the open pass, swapped while a capture is isolated
    struct RecordingContext {
        State state;
        u32 num_renderpass_images;
        std::array<VkImage, 9> renderpass_images;
        std::array<VkImageSubresourceRange, 9> renderpass_image_ranges;
        bool pass_has_transform_feedback;
        bool deferred_wait_for_idle;
        bool fragment_work_unwaited;
    };

    [[nodiscard]] RecordingContext SaveContext() const;
    void LoadContext(const RecordingContext& context);

    /// Pomegrade: the work leaves for the worker while a capture is isolated: the capture so far is placed after
    /// the open pass and continues where it was made (capture hoisting)
    void AbortIsolation();

    /// Pomegrade: the open pass's prologue takes no more captures; the chunks held for it go to the worker
    void ClosePassPrologue();

    /// Begins a render pass for the given framebuffer, optionally with clear values.
    void BeginRenderPassImpl(const Framebuffer* framebuffer, VkRenderPass renderpass,
                             const VkClearValue* clear_values, u32 clear_value_count);

    /// If a deferred clear is pending.
    void RealizeDeferredClear();

    void WorkerThread(std::stop_token stop_token);

    void AllocateWorkerCommandBuffer();

    u64 SubmitExecution(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore);

    void AllocateNewContext();

    void EndPendingOperations();

    void EndRenderPass();

    void AcquireNewChunk();

    const Device& device;
    StateTracker& state_tracker;

    std::unique_ptr<MasterSemaphore> master_semaphore;
    std::unique_ptr<CommandPool> command_pool;

    VideoCommon::QueryCacheBase<QueryCacheParams>* query_cache = nullptr;

    vk::CommandBuffer current_cmdbuf;
    vk::CommandBuffer current_upload_cmdbuf;

    DeferredClear deferred_clear;

    std::unique_ptr<CommandChunk> chunk;
    std::function<void()> on_submit;

    State state;

    bool deferred_wait_for_idle = false;

    /// Pomegrade: the open render pass holds a transform feedback draw (MarkTransformFeedbackInPass)
    bool pass_has_transform_feedback = false;

    /// Pomegrade: a render pass ended whose fragment work the non-fragment stages have not waited for yet
    bool fragment_work_unwaited = false;

    u32 num_renderpass_images = 0;
    const Framebuffer* null_framebuffer = nullptr;

    // Pomegrade: capture hoisting (CanHoistIntoPass)
    bool isolating = false;
    bool isolation_left_pass = false; ///< the isolated capture recorded something outside a pass
    bool skip_query_hooks = false;
    std::shared_ptr<CommandList> isolated_commands;
    std::unique_ptr<RecordingContext> main_context;
    std::shared_ptr<CommandList> pass_prologue; ///< the open pass's, while it can take captures
    std::vector<std::unique_ptr<CommandChunk>> held_chunks; ///< recorded after the prologue, held until it closes
    std::vector<std::pair<u64, u64>> main_pass_reads;
    bool main_pass_writes_memory = false;
    VkFramebuffer last_ended_framebuffer{};
    u64 pomegrade_captures_hoisted = 0;
    u64 pomegrade_captures_kept = 0;
    u64 pomegrade_pass_resumes = 0;
    std::array<u64, 12> pomegrade_hoist_refusals{};
    std::array<VkImage, 9> renderpass_images{};
    std::array<VkImageSubresourceRange, 9> renderpass_image_ranges{};

    std::queue<std::unique_ptr<CommandChunk>> work_queue;
    std::vector<std::unique_ptr<CommandChunk>> chunk_reserve;
    std::mutex execution_mutex;
    std::mutex reserve_mutex;
    std::mutex queue_mutex;
    std::condition_variable_any event_cv;
    bool worker_waiting{}; ///< The worker is waiting for work (guarded by queue_mutex)
    std::jthread worker_thread;

    std::chrono::steady_clock::duration frame_interval{};
    std::chrono::steady_clock::time_point start_time{};
    double last_target_fps{};
    u64 max_frame_count{};
    u64 frame_counter{};

    // Pomegrade: TEMPORARY, GPU time of each render pass (Vulkan timestamps around it), summed by the pass's kind
    // (render area, attachment count, draws in it) and logged every 2 seconds: where the Mali's frame goes
    struct PomegradePendingPass {
        u32 pair;
        std::string kind;
    };
    static constexpr u32 POMEGRADE_PASS_PAIRS = 4096;
    vk::QueryPool pomegrade_pass_timestamps;
    double pomegrade_timestamp_period = 0.0;
    std::vector<u32> pomegrade_free_pairs;
    std::deque<PomegradePendingPass> pomegrade_pending_passes;
    std::map<std::string, std::pair<u64, u64>> pomegrade_pass_time; // kind -> {passes, ns}
    std::mutex pomegrade_pass_mutex;
    u32 pomegrade_pass_pair = ~u32{0};
    u32 pomegrade_pass_images = 0;
    u64 pomegrade_pass_draws = 0;
    u64 pomegrade_pass_draws_start = 0;
    u64 pomegrade_dropped = 0;
    std::chrono::steady_clock::time_point pomegrade_last_log{};
    void PomegradePassBegin(u32 images);
    void PomegradePassEnd();
    void PomegradeCollectPasses();
};

} // namespace Vulkan
