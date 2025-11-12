#include "vulkan_compute_shader.hpp"

namespace NH3D {

[[nodiscard]] std::pair<VkPipeline, VkPipelineLayout> VulkanComputeShader::create(const VkDevice device, const VkDescriptorSetLayout layout,
    const std::filesystem::path& computeShaderPath, const std::vector<VkPushConstantRange>& pushConstantRanges)
{
    const VkPipelineLayout pipelineLayout = createPipelineLayout(device, layout, pushConstantRanges);

    const VkShaderModule shaderModule = loadShader(device, computeShaderPath);

    VkPipelineShaderStageCreateInfo stageCreateInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName = "main" };

    VkComputePipelineCreateInfo pipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stageCreateInfo, .layout = pipelineLayout
    };

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan compute pipeline");
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);

    return { pipeline, pipelineLayout };
}

void VulkanComputeShader::release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)
{
    VulkanPipeline::release(rhi, pipeline, pipelineLayout);
}

void VulkanComputeShader::dispatch(VkCommandBuffer commandBuffer, const VkPipeline pipeline, const vec3i kernelSize)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdDispatch(commandBuffer, kernelSize.x, kernelSize.y, kernelSize.z);
}

}