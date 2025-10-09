#include "rendering/vulkan/vulkan_compute_pipeline.hpp"
#include "rendering/vulkan/vulkan_pipeline.hpp"

namespace NH3D {

VulkanComputePipeline::VulkanComputePipeline(VkDevice device, VkDescriptorSetLayout layout, std::filesystem::path computeShaderPath)
    : VulkanPipeline { device, layout }
{
    VkShaderModule module;
    if (!loadShader(device, computeShaderPath, module)) {
        release(device);
    }
    _shaderModules.emplace_back(module);

    VkPipelineShaderStageCreateInfo stageCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = _shaderModules[0],
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageCreateInfo,
        .layout = _pipelineLayout,
    };

    if (vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &_pipeline) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan compute pipeline");
    }
}

void VulkanComputePipeline::dispatch(VkCommandBuffer commandBuffer, Vec3i kernelSize)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);

    vkCmdDispatch(commandBuffer, kernelSize.x, kernelSize.y, kernelSize.z);
}

}