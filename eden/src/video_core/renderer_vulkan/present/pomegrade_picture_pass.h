// SPDX-FileCopyrightText: Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;

/**
 * Pomegrade: one full-screen picture filter (Rich colours or Deep black, chosen by its fragment shader), drawn into
 * its own image after anti-aliasing and before FSR, as FXAA is drawn. The fragment shader samples binding 0.
 */
class PomegradePicturePass final {
public:
    explicit PomegradePicturePass(const Device& device, MemoryAllocator& allocator,
                                  size_t image_count, VkExtent2D extent,
                                  std::span<const u32> fragment_spv);
    ~PomegradePicturePass();

    void Draw(Scheduler& scheduler, size_t image_index, VkImage* inout_image,
              VkImageView* inout_image_view);

private:
    void UploadImages(Scheduler& scheduler);

    const Device& m_device;
    const VkExtent2D m_extent;
    const u32 m_image_count;

    vk::ShaderModule m_vertex_shader{};
    vk::ShaderModule m_fragment_shader{};
    vk::DescriptorPool m_descriptor_pool{};
    vk::DescriptorSetLayout m_descriptor_set_layout{};
    vk::PipelineLayout m_pipeline_layout{};
    vk::Pipeline m_pipeline{};
    vk::RenderPass m_renderpass{};
    vk::Sampler m_sampler{};

    struct Image {
        vk::DescriptorSets descriptor_sets{};
        vk::Framebuffer framebuffer{};
        vk::Image image{};
        vk::ImageView image_view{};
    };
    std::vector<Image> m_images{};
    bool m_images_ready{};
};

} // namespace Vulkan
