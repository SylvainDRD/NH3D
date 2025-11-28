#include "vulkan_pipeline.hpp"
#include <filesystem>
#include <rendering/vulkan/vulkan_rhi.hpp>

namespace NH3D {

void VulkanPipeline::release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (pipelineLayout) {
        vkDestroyPipelineLayout(vrhi.getVkDevice(), pipelineLayout, nullptr);
        pipelineLayout = nullptr;
    }

    if (pipeline) {
        vkDestroyPipeline(vrhi.getVkDevice(), pipeline, nullptr);
        pipeline = nullptr;
    }
}

VkPipelineLayout VulkanPipeline::createPipelineLayout(VkDevice device, const ArrayWrapper<VkDescriptorSetLayout> descriptorSetsLayouts,
    const ArrayWrapper<VkPushConstantRange> pushConstantRanges)
{
    const VkPipelineLayoutCreateInfo layoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptorSetsLayouts.size),
        .pSetLayouts = descriptorSetsLayouts.data,
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size),
        .pPushConstantRanges = pushConstantRanges.data,
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create the pipeline layout");
    }
    return pipelineLayout;
}

VkShaderModule VulkanPipeline::loadShaderModule(VkDevice device, const std::filesystem::path& path)
{
    if (!path.has_filename() || !std::filesystem::exists(path)) {
        NH3D_ABORT("Shader at \"" << path << "\" does not exist");
    }

    std::ifstream file { path, std::ios::ate | std::ios::binary };
    if (!file.is_open()) {
        NH3D_ABORT("Failed to open shader at \"" << path << "\"");
    }

    const size_t size = file.tellg();
    file.seekg(0);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    file.close();

    const VkShaderModuleCreateInfo moduleCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = reinterpret_cast<uint32_t*>(buffer.data())
    };

    VkShaderModule shader;
    if (vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shader) != VK_SUCCESS) {
        NH3D_ABORT("Failed to create shader module from \"" << path << "\"");
    }
    return shader;
}

}
