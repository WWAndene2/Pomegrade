// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <limits>
#include <mutex>
#include <vector>
#include "common/logging/log.h"
#include "video_core/frame_generation.h"
#include "video_core/renderer_vulkan/vk_frame_generation.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

#include <vk_mem_alloc.h>

namespace Vulkan {

namespace FG = VideoCore::FrameGeneration;

namespace {

struct PushConstants {
    float p0[4];
    float p1[4];
};

constexpr vk::Format LUMA_FORMAT = vk::Format::eR8G8Unorm;
constexpr vk::Format MOTION_FORMAT = vk::Format::eR16G16B16A16Sfloat;

u32 Small(u32 size) {
    return std::max<u32>(1, (size + 7) / 8);
}

constexpr vk::ImageSubresourceRange COLOR_RANGE{
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
};

vk::ImageMemoryBarrier Barrier(vk::Image image, vk::AccessFlags src, vk::AccessFlags dst, vk::ImageLayout from,
                               vk::ImageLayout to) {
    return vk::ImageMemoryBarrier{
        .srcAccessMask = src,
        .dstAccessMask = dst,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = COLOR_RANGE,
    };
}

} // namespace

FrameGeneratorVK::FrameGeneratorVK(const Instance& instance_, Scheduler& scheduler_,
                                   vk::RenderPass present_renderpass_, vk::Format format_)
    : instance{instance_}, scheduler{scheduler_}, device{instance_.GetDevice()},
      present_renderpass{present_renderpass_}, format{format_} {
    luma_pass = MakeRenderPass(LUMA_FORMAT);
    motion_pass = MakeRenderPass(MOTION_FORMAT);

    std::array<vk::DescriptorSetLayoutBinding, 3> bindings;
    for (u32 i = 0; i < 3; i++) {
        bindings[i] = {i, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment};
    }
    set_layout = device.createDescriptorSetLayout({
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    });
    const vk::PushConstantRange push_range{
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(PushConstants),
    };
    pipeline_layout = device.createPipelineLayout({
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    });
    downsample = MakePipeline(FG::DOWNSAMPLE_FRAG, luma_pass);
    estimate = MakePipeline(FG::MOTION_FRAG, motion_pass);
    warp = MakePipeline(FG::WARP_FRAG, present_renderpass);

    sampler = device.createSampler({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .maxLod = 0.0f,
    });
    const vk::DescriptorPoolSize pool_size{vk::DescriptorType::eCombinedImageSampler, 18};
    descriptor_pool = device.createDescriptorPool({
        .maxSets = 6,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    });
    const std::array<vk::DescriptorSetLayout, 6> layouts{set_layout, set_layout, set_layout,
                                                         set_layout, set_layout, set_layout};
    const auto allocated = device.allocateDescriptorSets({
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
    });
    for (u32 i = 0; i < 6; i++) {
        sets[i / 3][i % 3] = allocated[i];
    }

    command_pool = device.createCommandPool({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex(),
    });
    const auto cmdbufs = device.allocateCommandBuffers({
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 3,
    });
    copy_cmdbuf = cmdbufs[0];
    generate_cmdbuf = cmdbufs[1];
    generated = new Frame{};
    generated->cmdbuf = cmdbufs[2];
    generated->render_ready = device.createSemaphore({});
    generated->present_done = device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
    copy_done = device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
}

FrameGeneratorVK::~FrameGeneratorVK() {
    {
        // vkDeviceWaitIdle needs every queue to itself: the renderer submits under this lock
        std::scoped_lock lock{scheduler.submit_mutex};
        device.waitIdle();
    }
    DestroyImages();
    device.destroySemaphore(generated->render_ready);
    device.destroyFence(generated->present_done);
    delete generated;
    device.destroyFence(copy_done);
    device.destroyCommandPool(command_pool);
    device.destroyDescriptorPool(descriptor_pool);
    device.destroySampler(sampler);
    device.destroyPipeline(downsample);
    device.destroyPipeline(estimate);
    device.destroyPipeline(warp);
    device.destroyPipelineLayout(pipeline_layout);
    device.destroyDescriptorSetLayout(set_layout);
    device.destroyRenderPass(luma_pass);
    device.destroyRenderPass(motion_pass);
}

vk::RenderPass FrameGeneratorVK::MakeRenderPass(vk::Format target) {
    const vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
    const vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };
    const vk::AttachmentDescription attachment{
        .format = target,
        .loadOp = vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    return device.createRenderPass({
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    });
}

vk::Pipeline FrameGeneratorVK::MakePipeline(const char* fragment, vk::RenderPass pass) {
    const vk::ShaderModule vert = Compile(FG::Source(FG::FULLSCREEN_VERT, true), vk::ShaderStageFlagBits::eVertex,
                                          device);
    const vk::ShaderModule frag = Compile(FG::Source(fragment, true), vk::ShaderStageFlagBits::eFragment, device);
    const std::array stages{
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = vert, .pName = "main"},
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = frag, .pName = "main"},
    };
    const vk::PipelineVertexInputStateCreateInfo vertex_input{};
    const vk::PipelineInputAssemblyStateCreateInfo input_assembly{.topology = vk::PrimitiveTopology::eTriangleList};
    const vk::PipelineViewportStateCreateInfo viewport{.viewportCount = 1, .scissorCount = 1};
    const vk::PipelineRasterizationStateCreateInfo rasterization{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .lineWidth = 1.0f,
    };
    const vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1};
    const vk::PipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineColorBlendStateCreateInfo blend{.attachmentCount = 1, .pAttachments = &blend_attachment};
    const std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamic{
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
    const vk::GraphicsPipelineCreateInfo info{
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = pipeline_layout,
        .renderPass = pass,
    };
    const auto result = device.createGraphicsPipeline({}, info);
    device.destroyShaderModule(vert);
    device.destroyShaderModule(frag);
    if (result.result != vk::Result::eSuccess) {
        LOG_CRITICAL(Render_Vulkan, "Frame generation pipeline creation failed");
        UNREACHABLE();
    }
    return result.value;
}

FrameGeneratorVK::Image FrameGeneratorVK::MakeImage(vk::Format image_format, u32 w, u32 h,
                                                    vk::ImageUsageFlags usage, vk::RenderPass pass) {
    Image out;
    const VkImageCreateInfo image_info = static_cast<VkImageCreateInfo>(vk::ImageCreateInfo{
        .imageType = vk::ImageType::e2D,
        .format = image_format,
        .extent = {w, h, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
    });
    const VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };
    VkImage image{};
    if (vmaCreateImage(instance.GetAllocator(), &image_info, &alloc_info, &image, &out.allocation, nullptr) !=
        VK_SUCCESS) {
        LOG_CRITICAL(Render_Vulkan, "Frame generation image allocation failed");
        UNREACHABLE();
    }
    out.image = vk::Image{image};
    out.view = device.createImageView({
        .image = out.image,
        .viewType = vk::ImageViewType::e2D,
        .format = image_format,
        .subresourceRange = COLOR_RANGE,
    });
    if (pass) {
        out.framebuffer = device.createFramebuffer({
            .renderPass = pass,
            .attachmentCount = 1,
            .pAttachments = &out.view,
            .width = w,
            .height = h,
            .layers = 1,
        });
    }
    return out;
}

void FrameGeneratorVK::DestroyImage(Image& image) {
    if (image.framebuffer) {
        device.destroyFramebuffer(image.framebuffer);
    }
    if (image.view) {
        device.destroyImageView(image.view);
    }
    if (image.image) {
        vmaDestroyImage(instance.GetAllocator(), image.image, image.allocation);
    }
    image = Image{};
}

void FrameGeneratorVK::DestroyImages() {
    for (Image& image : history) {
        DestroyImage(image);
    }
    DestroyImage(luma);
    DestroyImage(motion);
    if (generated->framebuffer) {
        device.destroyFramebuffer(generated->framebuffer);
        device.destroyImageView(generated->image_view);
        vmaDestroyImage(instance.GetAllocator(), generated->image, generated->allocation);
        generated->framebuffer = vk::Framebuffer{};
    }
}

void FrameGeneratorVK::Resize(u32 w, u32 h) {
    {
        // vkDeviceWaitIdle needs every queue to itself: the renderer submits under this lock
        std::scoped_lock lock{scheduler.submit_mutex};
        device.waitIdle();
    }
    DestroyImages();
    width = w;
    height = h;
    for (Image& image : history) {
        image = MakeImage(format, w, h, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, {});
    }
    const vk::ImageUsageFlags target = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    luma = MakeImage(LUMA_FORMAT, Small(w), Small(h), target, luma_pass);
    motion = MakeImage(MOTION_FORMAT, Small(w), Small(h), target, motion_pass);
    // the generated images' frame, as PresentWindow::RecreateFrame makes the renderer's
    Image result = MakeImage(format, w, h,
                             vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                             present_renderpass);
    generated->image = result.image;
    generated->allocation = result.allocation;
    generated->image_view = result.view;
    generated->framebuffer = result.framebuffer;
    generated->width = w;
    generated->height = h;

    // [newest][pass]: A is history[1 - newest], B history[newest]; the motion pass reads the luma, the warp the motion
    for (int b = 0; b < 2; b++) {
        const std::array<vk::ImageView, 3> aux{history[b].view, luma.view, motion.view};
        for (int pass = 0; pass < 3; pass++) {
            const std::array<vk::DescriptorImageInfo, 3> infos{
                vk::DescriptorImageInfo{sampler, history[1 - b].view, vk::ImageLayout::eShaderReadOnlyOptimal},
                vk::DescriptorImageInfo{sampler, history[b].view, vk::ImageLayout::eShaderReadOnlyOptimal},
                vk::DescriptorImageInfo{sampler, aux[pass], vk::ImageLayout::eShaderReadOnlyOptimal},
            };
            device.updateDescriptorSets(
                vk::WriteDescriptorSet{
                    .dstSet = sets[b][pass],
                    .dstBinding = 0,
                    .descriptorCount = 3,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = infos.data(),
                },
                {});
        }
    }
    // the history and the small images start in the layout the passes read them in
    const vk::CommandBuffer cmdbuf = copy_cmdbuf;
    (void)device.waitForFences(copy_done, true, std::numeric_limits<u64>::max());
    device.resetFences(copy_done);
    cmdbuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const std::array barriers{
        Barrier(history[0].image, {}, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined,
                vk::ImageLayout::eShaderReadOnlyOptimal),
        Barrier(history[1].image, {}, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined,
                vk::ImageLayout::eShaderReadOnlyOptimal),
        Barrier(luma.image, {}, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined,
                vk::ImageLayout::eShaderReadOnlyOptimal),
        Barrier(motion.image, {}, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eUndefined,
                vk::ImageLayout::eShaderReadOnlyOptimal),
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader,
                           {}, {}, {}, barriers);
    cmdbuf.end();
    Submit(cmdbuf, {}, {}, copy_done);
    newest = -1;
}

void FrameGeneratorVK::Submit(vk::CommandBuffer cmdbuf, std::span<const vk::Semaphore> wait,
                              std::span<const vk::Semaphore> signal, vk::Fence fence) {
    const std::vector<vk::PipelineStageFlags> stages(wait.size(), vk::PipelineStageFlagBits::eAllCommands);
    const vk::SubmitInfo submit{
        .waitSemaphoreCount = static_cast<u32>(wait.size()),
        .pWaitSemaphores = wait.data(),
        .pWaitDstStageMask = stages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = static_cast<u32>(signal.size()),
        .pSignalSemaphores = signal.data(),
    };
    std::scoped_lock lock{scheduler.submit_mutex};
    instance.GetGraphicsQueue().submit(submit, fence);
}

void FrameGeneratorVK::CopyIn(Frame* frame, int slot) {
    (void)device.waitForFences(copy_done, true, std::numeric_limits<u64>::max());
    device.resetFences(copy_done);
    const vk::CommandBuffer cmdbuf = copy_cmdbuf;
    cmdbuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const vk::Image target = history[slot].image;
    const std::array before{
        // the renderer's frame, left by its render pass in the layout CopyToSwapchain reads it in
        Barrier(frame->image, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal),
        Barrier(target, vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal),
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eFragmentShader,
                           vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, before);
    const vk::ImageCopy region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = {width, height, 1},
    };
    cmdbuf.copyImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, target, vk::ImageLayout::eTransferDstOptimal,
                     region);
    const auto after = Barrier(target, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                               vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {},
                           {}, after);
    cmdbuf.end();
    // the frame is still to be presented after: its render_ready is waited on here and signalled again for
    // CopyToSwapchain
    const std::array semaphore{frame->render_ready};
    Submit(cmdbuf, semaphore, semaphore, copy_done);
}

void FrameGeneratorVK::Generate(float t) {
    // the generated frame's previous image must be on the screen before it is drawn over
    (void)device.waitForFences(generated->present_done, true, std::numeric_limits<u64>::max());
    const vk::CommandBuffer cmdbuf = generate_cmdbuf;
    cmdbuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const u32 sw = Small(width), sh = Small(height);
    const PushConstants push{
        {t, 1.0f / static_cast<float>(sw), 1.0f / static_cast<float>(sh), 0.0f},
        {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height), 0.0f, 0.0f},
    };
    const auto pass = [&](vk::RenderPass render_pass, vk::Framebuffer framebuffer, vk::Pipeline pipeline,
                          vk::DescriptorSet set, u32 w, u32 h) {
        cmdbuf.beginRenderPass({.renderPass = render_pass,
                                .framebuffer = framebuffer,
                                .renderArea = {{0, 0}, {w, h}}},
                               vk::SubpassContents::eInline);
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, set, {});
        cmdbuf.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(push), &push);
        cmdbuf.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f});
        cmdbuf.setScissor(0, vk::Rect2D{{0, 0}, {w, h}});
        cmdbuf.draw(3, 1, 0, 0);
        cmdbuf.endRenderPass();
    };
    // each pass reads what the one before wrote (the history copy came before in this queue)
    const vk::MemoryBarrier written{.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                                     vk::AccessFlagBits::eTransferWrite,
                                    .dstAccessMask = vk::AccessFlagBits::eShaderRead};
    const auto wait_written = [&] {
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eFragmentShader, {}, written, {}, {});
    };
    wait_written();
    pass(luma_pass, luma.framebuffer, downsample, sets[newest][0], sw, sh);
    wait_written();
    pass(motion_pass, motion.framebuffer, estimate, sets[newest][1], sw, sh);
    wait_written();
    // the present render pass leaves the image in the layout CopyToSwapchain reads it in
    pass(present_renderpass, generated->framebuffer, warp, sets[newest][2], width, height);
    cmdbuf.end();
    device.resetFences(generated->present_done);
    const std::array signal{generated->render_ready};
    Submit(cmdbuf, {}, signal, {});
}

void FrameGeneratorVK::Reset() {
    newest = -1;
}

void FrameGeneratorVK::Present(Frame* frame, const std::function<void(Frame*)>& present) {
    if (frame->width != width || frame->height != height) {
        Resize(frame->width, frame->height);
    }
    const int slot = newest < 0 ? 0 : 1 - newest;
    CopyIn(frame, slot);
    u32 images_between = 0;
    if (newest >= 0) {
        images_between = FG::ImagesBetween(static_cast<double>(frame->time_us - time_b_us) / 1e6);
    }
    newest = slot;
    time_b_us = frame->time_us;
    // the images between the previous frame (A) and this one (B), then B; FIFO presentation waits for the screen
    for (u32 i = 1; i <= images_between; i++) {
        Generate(static_cast<float>(i) / static_cast<float>(images_between + 1));
        present(generated);
    }
    present(frame);
}

} // namespace Vulkan
