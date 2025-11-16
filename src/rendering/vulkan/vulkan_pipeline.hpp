#pragma once

#include <filesystem>
#include <fstream>
#include <misc/utils.hpp>
#include <rendering/core/rhi.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace NH3D {

struct VulkanPipeline {
public:
    using HotType = VkPipeline;

    using ColdType = VkPipelineLayout;

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout);

protected:
    static VkPipelineLayout createPipelineLayout(VkDevice device, const ArrayWrapper<VkDescriptorSetLayout> descriptorSetsLayouts,
        const ArrayWrapper<VkPushConstantRange> pushConstantRanges);

    static VkShaderModule loadShaderModule(VkDevice device, const std::filesystem::path& path);
};

}