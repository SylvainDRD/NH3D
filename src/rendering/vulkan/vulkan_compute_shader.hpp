#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/compute_shader.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

struct VulkanComputeShader : public VulkanPipeline {
    using ResourceType = ComputeShader;

    /// "Constructor"
    [[nodiscard]] static std::pair<VkPipeline, VkPipelineLayout> create(const VkDevice device,
        const ArrayPtr<VkDescriptorSetLayout> descriptorSetsLayouts, const std::filesystem::path& computeShaderPath,
        const ArrayPtr<VkPushConstantRange> pushConstantRanges = { nullptr, 0 });

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout);

    // Used generically by the ResourceManager, must be API agnostic
    static bool valid(const VkPipeline pipeline, const VkPipelineLayout layout);

    static void dispatch(VkCommandBuffer commandBuffer, const VkPipeline pipeline, const vec3i kernelSize);
};

}