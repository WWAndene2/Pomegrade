// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <span>
#include "video_core/renderer_vulkan/vk_common.h"

VK_DEFINE_HANDLE(VmaAllocation)

namespace Vulkan {

class Instance;
class Scheduler;
struct Frame;

// Pomegrade: images generated between two distinct frames for the presentation thread (VideoCore::FrameGeneration).
// Each distinct frame B is copied into a history of two; the images between the previous one (A) and B are generated
// (downsample, motion, warp, the shaders of video_core/frame_generation.cpp) into a frame of their own and presented
// before B, each through PresentWindow::CopyToSwapchain, so FIFO presentation paces them at the screen's rate. What is
// shown runs one distinct frame late.
class FrameGeneratorVK {
public:
    FrameGeneratorVK(const Instance& instance, Scheduler& scheduler, vk::RenderPass present_renderpass,
                     vk::Format format);
    ~FrameGeneratorVK();

    /// Presents `frame` (made by the renderer, its render_ready semaphore pending) with the images generated before
    /// it, each through `present` (CopyToSwapchain)
    void Present(Frame* frame, const std::function<void(Frame*)>& present);

    /// Forgets the history (generation turned off): the next frame starts it again
    void Reset();

private:
    struct Image {
        vk::Image image;
        VmaAllocation allocation{};
        vk::ImageView view;
        vk::Framebuffer framebuffer;
    };

    void Resize(u32 width, u32 height);
    void DestroyImages();
    Image MakeImage(vk::Format format, u32 width, u32 height, vk::ImageUsageFlags usage, vk::RenderPass pass);
    void DestroyImage(Image& image);
    vk::RenderPass MakeRenderPass(vk::Format format);
    vk::Pipeline MakePipeline(const char* fragment, vk::RenderPass pass);
    void CopyIn(Frame* frame, int slot);
    void Generate(float t);
    void Submit(vk::CommandBuffer cmdbuf, std::span<const vk::Semaphore> wait,
                std::span<const vk::Semaphore> signal, vk::Fence fence);

    const Instance& instance;
    Scheduler& scheduler;
    vk::Device device;
    vk::RenderPass present_renderpass;
    vk::Format format;
    vk::RenderPass luma_pass, motion_pass;
    vk::DescriptorSetLayout set_layout;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline downsample, estimate, warp;
    vk::Sampler sampler;
    vk::DescriptorPool descriptor_pool;
    std::array<std::array<vk::DescriptorSet, 3>, 2> sets{}; ///< [newest][pass]: A and B swap with each frame
    vk::CommandPool command_pool;
    vk::CommandBuffer copy_cmdbuf, generate_cmdbuf;
    vk::Fence copy_done;
    std::array<Image, 2> history{};
    Image luma, motion;
    Frame* generated = nullptr; ///< the frame generated images are made in and presented from
    u32 width = 0, height = 0;
    int newest = -1;
    s64 time_b_us = 0;
};

} // namespace Vulkan
