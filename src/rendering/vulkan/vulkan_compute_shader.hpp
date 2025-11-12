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
    [[nodiscard]] static std::pair<Pipeline, PipelineLayout> create(const VkDevice device, const VkDescriptorSetLayout layout,
        const std::filesystem::path& computeShaderPath, const std::vector<VkPushConstantRange>& pushConstantRanges = {});

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, Pipeline& pipeline, PipelineLayout& pipelineLayout);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const Pipeline pipeline, const PipelineLayout layout)
    {
        return pipeline.pipeline != nullptr && layout.layout != nullptr;
    }

    static void dispatch(VkCommandBuffer commandBuffer, const Pipeline pipeline, const vec3i kernelSize);
};

}