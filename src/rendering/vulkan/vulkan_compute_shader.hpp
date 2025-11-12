#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/compute_shader.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

struct VulkanComputeShader : public VulkanPipeline {
    using ResourceType = ComputeShader;

    using Hot = VkPipeline;

    using Cold = VkPipelineLayout;

    /// "Constructor"
    [[nodiscard]] static std::pair<VkPipeline, VkPipelineLayout> create(const VkDevice device, const VkDescriptorSetLayout layout,
        const std::filesystem::path& computeShaderPath, const std::vector<VkPushConstantRange>& pushConstantRanges = {});

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const VkPipeline pipeline, const VkPipelineLayout layout) { return pipeline != nullptr && layout != nullptr; }

    static inline void dispatch(VkCommandBuffer commandBuffer, const VkPipeline pipeline, const vec3i kernelSize);
};

}