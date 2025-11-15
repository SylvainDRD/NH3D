#include "vulkan_compute_shader.hpp"

namespace NH3D {

[[nodiscard]] std::pair<VkPipeline, VkPipelineLayout> VulkanComputeShader::create(const VkDevice device, const ArrayPtr<VkDescriptorSetLayout> descriptorSetsLayouts,
    const std::filesystem::path& computeShaderPath, const ArrayPtr<VkPushConstantRange> pushConstantRanges)
{
    const VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetsLayouts, pushConstantRanges);

    const VkShaderModule shaderModule = loadShaderModule(device, computeShaderPath);

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

bool VulkanComputeShader::valid(const VkPipeline pipeline, const VkPipelineLayout layout)
{
    return pipeline != nullptr && layout != nullptr;
}

void VulkanComputeShader::dispatch(VkCommandBuffer commandBuffer, const VkPipeline pipeline, const vec3i kernelSize)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    vkCmdDispatch(commandBuffer, kernelSize.x, kernelSize.y, kernelSize.z);
}

}