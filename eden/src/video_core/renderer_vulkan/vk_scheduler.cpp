// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <tuple>
#include <algorithm>
#include <mutex>
#include <thread>
#include <utility>

#include <fmt/format.h>

#include "video_core/renderer_vulkan/vk_query_cache.h"

#include "common/settings.h"
#include "common/thread.h"
#include "video_core/gpu_logging/gpu_logging.h"
#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

void Scheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf,
                                         vk::CommandBuffer upload_cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf, upload_cmdbuf);
        command->~Command();
        command = next;
    }
    submit = false;
    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

Scheduler::Scheduler(const Device& device_, StateTracker& state_tracker_)
    : device{device_}, state_tracker{state_tracker_},
      master_semaphore{std::make_unique<MasterSemaphore>(device)},
      command_pool{std::make_unique<CommandPool>(*master_semaphore, device)} {

    // Pomegrade: TEMPORARY, the per-pass GPU timer (vk_scheduler.h), when the queue writes timestamps
    const auto limits = device.GetPhysical().GetProperties().limits;
    if (limits.timestampComputeAndGraphics) {
        pomegrade_pass_timestamps = device.GetLogical().CreateQueryPool({
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = POMEGRADE_PASS_PAIRS * 2,
            .pipelineStatistics = 0,
        });
        device.GetLogical().ResetQueryPool(*pomegrade_pass_timestamps, 0, POMEGRADE_PASS_PAIRS * 2);
        pomegrade_timestamp_period = limits.timestampPeriod;
        for (u32 pair = POMEGRADE_PASS_PAIRS; pair-- > 0;) {
            pomegrade_free_pairs.push_back(pair);
        }
    } else {
        LOG_INFO(Render_Vulkan, "[Pomegrade pass time] no timestamps on this queue");
    }

    AcquireNewChunk();
    AllocateWorkerCommandBuffer();
    worker_thread = std::jthread([this](std::stop_token token) { WorkerThread(token); });
}

Scheduler::~Scheduler() = default;

u64 Scheduler::Flush(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    if (VideoCommon::GpuSplit::check_owner) [[unlikely]] {
        VideoCommon::GpuSplit::CheckOwner("Scheduler::Flush");
    }
    // When flushing, we only send data to the worker thread; no waiting is necessary.
    const u64 signal_value = SubmitExecution(signal_semaphore, wait_semaphore);
    AllocateNewContext();
    return signal_value;
}

void Scheduler::Finish(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    if (VideoCommon::GpuSplit::check_owner) [[unlikely]] {
        VideoCommon::GpuSplit::CheckOwner("Scheduler::Finish");
    }
    // When finishing, we need to wait for the submission to have executed on the device.
    const u64 presubmit_tick = CurrentTick();
    SubmitExecution(signal_semaphore, wait_semaphore);
    Wait(presubmit_tick);
    AllocateNewContext();
}

void Scheduler::WaitWorker() {
    if (isolating) {
        AbortIsolation();
    }
    ClosePassPrologue();
    DispatchWork();

    // Ensure the queue is drained.
    {
        std::unique_lock ql{queue_mutex};
        event_cv.wait(ql, [this] { return work_queue.empty(); });
    }

    // Now wait for execution to finish.
    std::scoped_lock el{execution_mutex};
}

void Scheduler::DispatchWork() {
    if (VideoCommon::GpuSplit::check_owner) [[unlikely]] {
        VideoCommon::GpuSplit::CheckOwner("Scheduler::DispatchWork");
    }
    if (isolating) [[unlikely]] {
        AbortIsolation();
    }
    if (pass_prologue && chunk && !chunk->Empty()) {
        // Pomegrade: the open pass's prologue is in this work, still taking captures: held until it closes
        held_chunks.push_back(std::move(chunk));
        AcquireNewChunk();
        return;
    }
    if (chunk && !chunk->Empty()) {
        // Waking the worker is a system call (~2% of the GPU thread at one dispatch per 8
        // draws): skip it while the worker is still busy, it pops the queue before waiting.
        bool wake;
        {
            std::scoped_lock ql{queue_mutex};
            work_queue.push(std::move(chunk));
            wake = worker_waiting;
        }
        if (wake) {
            event_cv.notify_all();
        }
        AcquireNewChunk();
    }
}

void Scheduler::BeginRenderPassImpl(const Framebuffer* framebuffer, VkRenderPass renderpass,
                                    const VkClearValue* clear_values, u32 clear_value_count) {
    const VkFramebuffer framebuffer_handle = framebuffer->Handle();
    const VkExtent2D render_area = framebuffer->RenderArea();
    if (!isolating) {
        if (framebuffer->NumImages() != 0 && framebuffer_handle == last_ended_framebuffer) {
            ++pomegrade_pass_resumes; // Pomegrade: TEMPORARY, the same attachments ended and began again
        }
        ClosePassPrologue();
        main_pass_reads.clear();
        main_pass_writes_memory = false;
        if (framebuffer->NumImages() != 0 && Settings::values.pomegrade_hoist_captures.GetValue()) {
            // Pomegrade: the space before this pass for the captures made while it is open (CanHoistIntoPass).
            // Its draws are recorded after the begin with all their state, since the captures placed before
            // it change that state
            pass_prologue = std::make_shared<CommandList>();
            RecordWithUploadBuffer([prologue = pass_prologue](vk::CommandBuffer cmdbuf,
                                                              vk::CommandBuffer upload_cmdbuf) {
                prologue->Execute(cmdbuf, upload_cmdbuf);
            });
            InvalidateState();
        }
    }
    state.renderpass = renderpass;
    state.framebuffer = framebuffer_handle;
    state.render_area = render_area;
    PomegradePassBegin(framebuffer->NumImages()); // Pomegrade: TEMPORARY, before the pass is recorded

    if (GPU::Logging::IsActive() && Settings::values.gpu_log_vulkan_calls.GetValue()) {
        const std::string render_pass_info =
            fmt::format("renderArea={}x{}, numImages={}", render_area.width, render_area.height,
                        framebuffer->NumImages());
        GPU::Logging::GPULogger::GetInstance().LogRenderPassBegin(render_pass_info);
    }

    std::array<VkClearValue, 9> values{};
    for (u32 i = 0; i < clear_value_count && i < values.size(); ++i) {
        values[i] = clear_values[i];
    }
    Record([renderpass, framebuffer_handle, render_area, values, clear_value_count](
               vk::CommandBuffer cmdbuf) {
        const VkRenderPassBeginInfo renderpass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .framebuffer = framebuffer_handle,
            .renderArea =
                {
                    .offset = {.x = 0, .y = 0},
                    .extent = render_area,
                },
            .clearValueCount = clear_value_count,
            .pClearValues = clear_value_count != 0 ? values.data() : nullptr,
        };
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
    });
    num_renderpass_images = framebuffer->NumImages();
    renderpass_images = framebuffer->Images();
    renderpass_image_ranges = framebuffer->ImageRanges();
    framebuffer->MarkResolveShadowsUpToDate();
}

void Scheduler::RealizeDeferredClear() {
    if (deferred_clear.framebuffer == nullptr) {
        return;
    }
    const DeferredClear dc = deferred_clear;
    deferred_clear = {};

    std::array<VkClearValue, 9> clear_values{};
    u32 count = 0;
    const RenderPassKey& base = dc.framebuffer->RenderPassKeyBase();
    for (u32 slot = 0; slot < 8; ++slot) {
        if (base.color_formats[slot] == VideoCore::Surface::PixelFormat::Invalid) {
            continue;
        }
        clear_values[count++] = dc.color_values[slot];
    }
    if (base.depth_format != VideoCore::Surface::PixelFormat::Invalid) {
        clear_values[count++] = dc.depth_stencil_value;
    }
    const u32 color_discard_mask =
        dc.framebuffer->DiscardsMsaaColor() ? dc.color_clear_mask : 0u;
    const bool depth_stencil_discard =
        dc.depth_stencil && dc.framebuffer->DiscardsMsaaDepthStencil();
    const VkRenderPass renderpass = dc.framebuffer->RenderPassVariant(
        dc.color_clear_mask, dc.depth_stencil, color_discard_mask, depth_stencil_discard);
    EndRenderPass();
    BeginRenderPassImpl(dc.framebuffer, renderpass, clear_values.data(), count);
}

bool Scheduler::DeferColorClear(const Framebuffer* framebuffer, u32 rt_slot,
                                const VkClearValue& value) {
    if (IsRenderPassActive()) {
        return false;
    }
    if (deferred_clear.framebuffer != nullptr && deferred_clear.framebuffer != framebuffer) {
        RealizeDeferredClear();
        EndRenderPass();
    }
    deferred_clear.framebuffer = framebuffer;
    deferred_clear.color_clear_mask |= 1u << rt_slot;
    deferred_clear.color_values[rt_slot] = value;
    return true;
}

bool Scheduler::DeferDepthStencilClear(const Framebuffer* framebuffer, const VkClearValue& value) {
    if (IsRenderPassActive()) {
        return false;
    }
    if (deferred_clear.framebuffer != nullptr && deferred_clear.framebuffer != framebuffer) {
        RealizeDeferredClear();
        EndRenderPass();
    }
    deferred_clear.framebuffer = framebuffer;
    deferred_clear.depth_stencil = true;
    deferred_clear.depth_stencil_value = value;
    return true;
}

void Scheduler::FlushDeferredClear() {
    if (deferred_clear.framebuffer == nullptr) {
        return;
    }
    RealizeDeferredClear();
    EndRenderPass();
}

void Scheduler::RequestRenderpass(const Framebuffer* framebuffer) {
    if (deferred_clear.framebuffer == framebuffer) {
        RealizeDeferredClear();
        return;
    }
    const VkRenderPass renderpass = framebuffer->RenderPass();
    const VkFramebuffer framebuffer_handle = framebuffer->Handle();
    const VkExtent2D render_area = framebuffer->RenderArea();
    if (renderpass == state.renderpass && framebuffer_handle == state.framebuffer &&
        render_area.width == state.render_area.width &&
        render_area.height == state.render_area.height) {
        return;
    }
    // Ends any active pass and realizes a deferred clear
    EndRenderPass();
    BeginRenderPassImpl(framebuffer, renderpass, nullptr, 0);
}

bool Scheduler::IsRenderPassOn(const Framebuffer* framebuffer) const noexcept {
    return state.renderpass != VkRenderPass{} && state.framebuffer == framebuffer->Handle();
}

void Scheduler::RequestOutsideRenderPassOperationContext() {
    if (isolating) {
        isolation_left_pass = true;
    }
    EndRenderPass();
}

Scheduler::RecordingContext Scheduler::SaveContext() const {
    return RecordingContext{state, num_renderpass_images, renderpass_images, renderpass_image_ranges,
                            pass_has_transform_feedback, deferred_wait_for_idle, fragment_work_unwaited};
}

void Scheduler::LoadContext(const RecordingContext& context) {
    state = context.state;
    num_renderpass_images = context.num_renderpass_images;
    renderpass_images = context.renderpass_images;
    renderpass_image_ranges = context.renderpass_image_ranges;
    pass_has_transform_feedback = context.pass_has_transform_feedback;
    deferred_wait_for_idle = context.deferred_wait_for_idle;
    fragment_work_unwaited = context.fragment_work_unwaited;
}

size_t Scheduler::HoistIntoPassRefusal() const noexcept {
    if (!pass_prologue) return 1;
    if (!IsRenderPassActive()) return 2;
    if (num_renderpass_images == 0) return 3;
    if (pass_has_transform_feedback) return 4;
    if (deferred_wait_for_idle) return 5;
    if (main_pass_writes_memory) return 6;
    if (deferred_clear.framebuffer != nullptr) return 7;
    return 0;
}

bool Scheduler::CanHoistIntoPass() const noexcept {
    return !isolating && pass_prologue != nullptr && IsRenderPassActive() && num_renderpass_images != 0 &&
           !pass_has_transform_feedback && !deferred_wait_for_idle && !main_pass_writes_memory &&
           deferred_clear.framebuffer == nullptr;
}

void Scheduler::BeginIsolation() {
    main_context = std::make_unique<RecordingContext>(SaveContext());
    isolated_commands = std::make_shared<CommandList>();
    isolation_left_pass = false;
    isolating = true;
    // the capture records as if no pass were open, with all its state
    state.renderpass = VkRenderPass{};
    state.framebuffer = VkFramebuffer{};
    state.render_area = {0, 0};
    num_renderpass_images = 0;
    pass_has_transform_feedback = false;
    deferred_wait_for_idle = false;
    InvalidateState();
}

void Scheduler::EndIsolation() {
    if (!isolating) {
        return; // aborted, the capture went on where it was made
    }
    EndRenderPass(); // the capture's own pass: transform feedback ended, its barrier after
    const bool hoist = !isolation_left_pass && pass_prologue != nullptr;
    std::shared_ptr<CommandList> commands = std::move(isolated_commands);
    isolating = false;
    LoadContext(*main_context);
    if (hoist) {
        pass_prologue->Append(std::move(*commands));
        ++pomegrade_captures_hoisted;
    } else {
        // as before: the open pass ends, the capture follows
        EndRenderPass();
        RecordWithUploadBuffer([commands](vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
            commands->Execute(cmdbuf, upload_cmdbuf);
        });
        ++pomegrade_captures_kept;
    }
    InvalidateState();
}

void Scheduler::AbortIsolation() {
    const RecordingContext isolated = SaveContext();
    std::shared_ptr<CommandList> commands = std::move(isolated_commands);
    isolating = false;
    LoadContext(*main_context);
    // the open pass had no capture, query or conditional rendering of its own (CanHoistIntoPass and the
    // rasterizer's checks): the query state now is the capture's, so its end must not touch it
    skip_query_hooks = true;
    EndRenderPass();
    skip_query_hooks = false;
    RecordWithUploadBuffer([commands](vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
        commands->Execute(cmdbuf, upload_cmdbuf);
    });
    LoadContext(isolated); // the capture's pass, if open, goes on in the work as it is
    ++pomegrade_captures_kept;
}

void Scheduler::ClosePassPrologue() {
    if (!pass_prologue) {
        return;
    }
    pass_prologue.reset();
    if (held_chunks.empty()) {
        return;
    }
    bool wake;
    {
        std::scoped_lock ql{queue_mutex};
        for (auto& held : held_chunks) {
            work_queue.push(std::move(held));
        }
        wake = worker_waiting;
    }
    held_chunks.clear();
    if (wake) {
        event_cv.notify_all();
    }
}

void Scheduler::NoteMainPassRead(u64 begin, u64 end) {
    if (isolating || !pass_prologue || begin >= end) {
        return;
    }
    if (!main_pass_reads.empty() && main_pass_reads.back() == std::pair{begin, end}) {
        return;
    }
    main_pass_reads.emplace_back(begin, end);
}

bool Scheduler::MainPassReadOverlaps(u64 begin, u64 end) const noexcept {
    for (const auto& [read_begin, read_end] : main_pass_reads) {
        if (begin < read_end && read_begin < end) {
            return true;
        }
    }
    return false;
}

void Scheduler::RequestNonFragmentDependency() {
    if (!fragment_work_unwaited) {
        return;
    }
    // The open pass began after the unwaited one ended, so the barrier must come before it: end it
    // (its own fragment work is then covered too) and let the draw begin a new one
    EndRenderPass();
    fragment_work_unwaited = false;
    Record([](vk::CommandBuffer cmdbuf) {
        static constexpr VkMemoryBarrier FRAGMENT_TO_NON_FRAGMENT{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                             VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
                             VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT,
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               vk::PIPELINE_STAGE_GRAPHICS_COMPUTE, 0, FRAGMENT_TO_NON_FRAGMENT);
    });
}

bool Scheduler::UpdateGraphicsPipeline(GraphicsPipeline* pipeline) {
    if (state.graphics_pipeline == pipeline) {
        if (pipeline && pipeline->UsesExtendedDynamicState() &&
            state.needs_state_enable_refresh) {
            state_tracker.InvalidateStateEnableFlag();
            state.needs_state_enable_refresh = false;
        }
        return false;
    }

    state.graphics_pipeline = pipeline;

    if (!pipeline) {
        return true;
    }

    if (!pipeline->UsesExtendedDynamicState()) {
        state.needs_state_enable_refresh = true;
    } else if (state.needs_state_enable_refresh) {
        state_tracker.InvalidateStateEnableFlag();
        state.needs_state_enable_refresh = false;
    }

    return true;
}

bool Scheduler::UpdateRescaling(bool is_rescaling) {
    if (state.rescaling_defined && is_rescaling == state.is_rescaling) {
        return false;
    }
    state.rescaling_defined = true;
    state.is_rescaling = is_rescaling;
    return true;
}

bool Scheduler::UpdateDescriptorBufferChunk(u32 descriptor_chunk) {
    if (state.descriptor_buffer_bound && descriptor_chunk == state.descriptor_buffer_chunk) {
        return false;
    }
    state.descriptor_buffer_bound = true;
    state.descriptor_buffer_chunk = descriptor_chunk;
    return true;
}

void Scheduler::WorkerThread(std::stop_token stop_token) {
    Common::SetCurrentThreadName("VulkanWorker");
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Critical);
    Common::SetCurrentThreadToPerformanceCores();

    const auto TryPopQueue{[this](auto& work) -> bool {
        if (work_queue.empty()) {
            return false;
        }

        work = std::move(work_queue.front());
        work_queue.pop();
        event_cv.notify_all();
        return true;
    }};

    while (!stop_token.stop_requested()) {
        std::unique_ptr<CommandChunk> work;

        {
            std::unique_lock lk{queue_mutex};

            // Wait for work.
            event_cv.wait(lk, stop_token, [&] {
                worker_waiting = !TryPopQueue(work);
                return !worker_waiting;
            });
            worker_waiting = false;

            // If we've been asked to stop, we're done.
            if (stop_token.stop_requested()) {
                return;
            }

            // Exchange lock ownership so that we take the execution lock before
            // the queue lock goes out of scope. This allows us to force execution
            // to complete in the next step.
            void(std::exchange(lk, std::unique_lock{execution_mutex}));

            // Perform the work, tracking whether the chunk was a submission
            // before executing.
            const bool has_submit = work->HasSubmit();
            work->ExecuteAll(current_cmdbuf, current_upload_cmdbuf);

            // If the chunk was a submission, reallocate the command buffer.
            if (has_submit) {
                AllocateWorkerCommandBuffer();
            }
        }

        {
            std::scoped_lock rl{reserve_mutex};

            // Recycle the chunk back to the reserve.
            chunk_reserve.emplace_back(std::move(work));
        }
    }
}

void Scheduler::AllocateWorkerCommandBuffer() {
    current_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });
    current_upload_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_upload_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });
}

u64 Scheduler::SubmitExecution(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    if (isolating) [[unlikely]] {
        AbortIsolation();
    }
    EndPendingOperations();
    InvalidateState();

    const u64 signal_value = master_semaphore->NextTick();
    RecordWithUploadBuffer([signal_semaphore, wait_semaphore, signal_value,
                            this](vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        };
        upload_cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, WRITE_BARRIER);
        upload_cmdbuf.End();
        cmdbuf.End();

        if (on_submit) {
            on_submit();
        }

        std::scoped_lock lock{submit_mutex};
        switch (const VkResult result = master_semaphore->SubmitQueue(
                    cmdbuf, upload_cmdbuf, signal_semaphore, wait_semaphore, signal_value)) {
        case VK_SUCCESS:
            // Log successful queue submission
            if (GPU::Logging::IsActive() &&
                Settings::values.gpu_log_vulkan_calls.GetValue()) {
                GPU::Logging::GPULogger::GetInstance().LogVulkanCall(
                    "vkQueueSubmit", "", VK_SUCCESS);
            }
            break;
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss();
            [[fallthrough]];
        default:
            vk::Check(result);
            break;
        }
    });
    chunk->MarkSubmit();
    DispatchWork();
    return signal_value;
}

void Scheduler::AllocateNewContext() {
    // Enable counters once again. These are disabled when a command buffer is finished.
}

void Scheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state.rescaling_defined = false;
    state.descriptor_buffer_bound = false;
    state_tracker.InvalidateCommandBufferState();
}

void Scheduler::EndPendingOperations() {
    query_cache->CounterReset(VideoCommon::QueryType::ZPassPixelCount64);
    EndRenderPass();
}

void Scheduler::EndRenderPass()
    {
        RealizeDeferredClear();
        if (!state.renderpass) {
            return;
        }

        if (!skip_query_hooks) {
            query_cache->CounterClose(VideoCommon::QueryType::StreamingByteCount);
        }

        // Log render pass end
        if (GPU::Logging::IsActive() &&
            Settings::values.gpu_log_vulkan_calls.GetValue()) {
            GPU::Logging::GPULogger::GetInstance().LogRenderPassEnd();
        }

        if (!skip_query_hooks) {
            query_cache->CounterEnable(VideoCommon::QueryType::ZPassPixelCount64, false);
            query_cache->NotifySegment(false);
        }

        // Pomegrade: a "wait for idle" deferred to the end of this pass (DeferWaitForIdle) waits here
        const bool wait_for_idle = deferred_wait_for_idle;
        deferred_wait_for_idle = false;
        Record([num_images = num_renderpass_images,
                       images = renderpass_images,
                       ranges = renderpass_image_ranges,
                       // Pomegrade: only after a pass that captured something; upstream added this
                       // barrier after every pass, a second vertex-waiting barrier on the Mali's 230
                       // to 500 passes a frame in Scarlet, most of which capture nothing
                       has_transform_feedback = device.IsExtTransformFeedbackSupported() &&
                                                pass_has_transform_feedback,
                       wait_for_idle](
                          vk::CommandBuffer cmdbuf) {
            std::array<VkImageMemoryBarrier, 9> barriers;
            for (size_t i = 0; i < num_images; ++i) {
                const VkImageSubresourceRange& range = ranges[i];
                const bool is_color = (range.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
                const bool is_depth_stencil = (range.aspectMask
                                              & (VK_IMAGE_ASPECT_DEPTH_BIT
                                                 | VK_IMAGE_ASPECT_STENCIL_BIT)) !=0;

                VkAccessFlags src_access = 0;

                if (is_color)
                    src_access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                else if (is_depth_stencil)
                    src_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                else
                    src_access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                barriers[i] = VkImageMemoryBarrier{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext = nullptr,
                        .srcAccessMask = src_access,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                                         | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                         | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = images[i],
                        .subresourceRange = range,
                };
            }
            cmdbuf.EndRenderPass();
            // Pomegrade: only the next passes' fragment stages wait here. Waiting with every graphics and
            // compute stage, as upstream does, kept the Mali from running one pass's vertex work beside
            // the previous pass's fragment work (Arm: "Avoid fragment to non-fragment barriers ... This
            // creates a pipeline bubble", Vulkan synchronization on Mali, 2026), at every one of
            // Scarlet's 230 to 500 passes a frame. Work in other stages that reads what a pass wrote
            // waits through RequestNonFragmentDependency: shaders of the vertex to geometry stages using
            // textures, images or storage buffers, and indirect dispatches. Direct dispatches and
            // transfers keep their own barriers from every graphics stage.
            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   0, nullptr, nullptr, vk::Span(barriers.data(), num_images));
            if (has_transform_feedback) {
                static constexpr VkMemoryBarrier XFB_OUTPUT_BARRIER{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
                    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                };
                cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                                       VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       0, XFB_OUTPUT_BARRIER);
            }
            if (wait_for_idle) {
                // everything recorded before waits for everything before it, as the guest asked
                static constexpr VkMemoryBarrier WAIT_FOR_IDLE{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                };
                cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, WAIT_FOR_IDLE);
            }
        });

        PomegradePassEnd(); // Pomegrade: TEMPORARY, after the pass's end is recorded
        // Pomegrade: a pass that wrote attachments leaves fragment work the non-fragment stages have not
        // waited for (a wait for idle above already covers everything); passes without attachments
        // (transform feedback captures) run no fragment work
        fragment_work_unwaited = wait_for_idle ? false : (fragment_work_unwaited || num_renderpass_images != 0);
        if (!isolating) {
            last_ended_framebuffer = num_renderpass_images != 0 ? state.framebuffer : VkFramebuffer{};
            ClosePassPrologue(); // after the pass's end, so held work keeps its order
        }
        state.renderpass = VkRenderPass{};
        pass_has_transform_feedback = false;
        num_renderpass_images = 0;
    }


void Scheduler::AcquireNewChunk() {
    std::scoped_lock rl{reserve_mutex};

    if (chunk_reserve.empty()) {
        // If we don't have anything reserved, we need to make a new chunk.
        chunk = std::make_unique<CommandChunk>();
    } else {
        // Otherwise, we can just take from the reserve.
        chunk = std::move(chunk_reserve.back());
        chunk_reserve.pop_back();
    }
}

// Pomegrade: TEMPORARY, the per-pass GPU timer (vk_scheduler.h). Begin and end timestamps are recorded around the
// pass's commands; results are read back without waiting, oldest first, once the GPU has written them.
void Scheduler::PomegradePassBegin(u32 images) {
    if (isolating) {
        return; // an isolated capture's pass: the open pass keeps its timer
    }
    pomegrade_pass_pair = ~u32{0};
    if (!pomegrade_pass_timestamps) {
        return;
    }
    PomegradeCollectPasses();
    u32 pair;
    {
        // not held while recording: the worker thread takes this mutex in the end timestamp's command
        std::scoped_lock lock{pomegrade_pass_mutex};
        if (pomegrade_free_pairs.empty()) {
            ++pomegrade_dropped;
            return;
        }
        pair = pomegrade_free_pairs.back();
        pomegrade_free_pairs.pop_back();
    }
    pomegrade_pass_pair = pair;
    pomegrade_pass_images = images;
    pomegrade_pass_draws_start = pomegrade_pass_draws;
    Record([this, pair](vk::CommandBuffer cmdbuf) {
        cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *pomegrade_pass_timestamps, pair * 2);
    });
}

void Scheduler::PomegradePassEnd() {
    if (isolating) {
        return;
    }
    const u32 pair = pomegrade_pass_pair;
    pomegrade_pass_pair = ~u32{0};
    if (pair == ~u32{0}) {
        return;
    }
    const u64 draws = pomegrade_pass_draws - pomegrade_pass_draws_start;
    const char* bucket = draws == 0    ? "0 draws"
                         : draws == 1  ? "1 draw"
                         : draws <= 10 ? "2-10 draws"
                         : draws <= 100 ? "11-100 draws"
                                        : ">100 draws";
    std::string kind = fmt::format("{}x{} {} images, {}", state.render_area.width, state.render_area.height,
                                   pomegrade_pass_images, bucket);
    Record([this, pair, kind = std::move(kind)](vk::CommandBuffer cmdbuf) {
        cmdbuf.WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, *pomegrade_pass_timestamps, pair * 2 + 1);
        std::scoped_lock lock{pomegrade_pass_mutex};
        pomegrade_pending_passes.push_back({pair, kind});
    });
}

void Scheduler::PomegradeCollectPasses() {
    std::scoped_lock lock{pomegrade_pass_mutex};
    while (!pomegrade_pending_passes.empty()) {
        const auto& pending = pomegrade_pending_passes.front();
        std::array<u64, 4> data{}; // begin, its availability, end, its availability
        const VkResult result = device.GetLogical().GetQueryResults(
            *pomegrade_pass_timestamps, pending.pair * 2, 2, sizeof(data), data.data(), sizeof(u64) * 2,
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if ((result != VK_SUCCESS && result != VK_NOT_READY) || data[1] == 0 || data[3] == 0) {
            break;
        }
        const u64 ns = data[2] > data[0]
                           ? static_cast<u64>(static_cast<double>(data[2] - data[0]) * pomegrade_timestamp_period)
                           : 0;
        auto& entry = pomegrade_pass_time[pending.kind];
        entry.first++;
        entry.second += ns;
        device.GetLogical().ResetQueryPool(*pomegrade_pass_timestamps, pending.pair * 2, 2);
        pomegrade_free_pairs.push_back(pending.pair);
        pomegrade_pending_passes.pop_front();
    }
    const auto now = std::chrono::steady_clock::now();
    if (pomegrade_last_log == std::chrono::steady_clock::time_point{}) {
        pomegrade_last_log = now;
        return;
    }
    if (now - pomegrade_last_log < std::chrono::seconds(2)) {
        return;
    }
    const double seconds = std::chrono::duration<double>(now - pomegrade_last_log).count();
    pomegrade_last_log = now;
    std::vector<std::tuple<u64, u64, std::string>> kinds;
    u64 total = 0;
    for (const auto& [kind, value] : pomegrade_pass_time) {
        kinds.emplace_back(value.second, value.first, kind);
        total += value.second;
    }
    pomegrade_pass_time.clear();
    std::sort(kinds.begin(), kinds.end(),
              [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
    std::string text;
    for (size_t i = 0; i < kinds.size() && i < 20; i++) {
        const auto& [ns, count, kind] = kinds[i];
        text += fmt::format(" [{}: {:.1f} ms/s, {:.0f} passes/s]", kind, ns / 1e6 / seconds, count / seconds);
    }
    LOG_INFO(Render_Vulkan, "[Pomegrade pass time] all passes {:.0f} ms/s (pairs dropped {}):{}",
             total / 1e6 / seconds, pomegrade_dropped, text);
    pomegrade_dropped = 0;
    // Pomegrade: TEMPORARY, capture hoisting at work: captures placed before the open pass, captures that still
    // ended it, and passes begun again on the attachments the pass before ended on (each a write out of tile
    // memory and a read back in on a Mali)
    LOG_INFO(Render_Vulkan, "[Pomegrade captures] hoisted {:.0f}/s, kept {:.0f}/s, passes resumed {:.0f}/s",
             pomegrade_captures_hoisted / seconds, pomegrade_captures_kept / seconds,
             pomegrade_pass_resumes / seconds);
    static constexpr std::array<const char*, 12> REASONS{
        "",          "no prologue", "no pass open", "pass without attachment", "pass captured",
        "wait for idle", "pass wrote memory", "clear pending", "not a null-pass capture",
        "occlusion count", "conditional rendering", "output read by the pass"};
    std::string refusals;
    for (size_t reason = 1; reason < REASONS.size(); ++reason) {
        if (pomegrade_hoist_refusals[reason] != 0) {
            refusals += fmt::format(" [{}: {:.0f}/s]", REASONS[reason], pomegrade_hoist_refusals[reason] / seconds);
        }
    }
    pomegrade_hoist_refusals.fill(0);
    LOG_INFO(Render_Vulkan, "[Pomegrade captures] refused:{}", refusals);
    pomegrade_captures_hoisted = 0;
    pomegrade_captures_kept = 0;
    pomegrade_pass_resumes = 0;
}

} // namespace Vulkan
