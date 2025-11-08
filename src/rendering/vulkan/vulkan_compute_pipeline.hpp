#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

class VulkanComputePipeline : public VulkanPipeline {
    NH3D_NO_COPY_MOVE(VulkanComputePipeline)
public:
    VulkanComputePipeline(VkDevice device, VkDescriptorSetLayout layout, const std::filesystem::path& computeShaderPath,
        const std::vector<VkPushConstantRange>& pushConstantRanges = {});

    void dispatch(VkCommandBuffer commandBuffer, vec3i kernelSize);
};

}