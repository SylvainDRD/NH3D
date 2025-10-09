#pragma once

#include <misc/utils.hpp>
#include <filesystem>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

class VulkanComputePipeline : public VulkanPipeline {
    NH3D_NO_COPY_MOVE(VulkanComputePipeline)
public:
    VulkanComputePipeline(VkDevice device, VkDescriptorSetLayout layout, std::filesystem::path computeShaderPath);

    void dispatch(VkCommandBuffer commandBuffer, Vec3i kernelSize);
};

}