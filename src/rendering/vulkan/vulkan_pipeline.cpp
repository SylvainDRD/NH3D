#include "vulkan_pipeline.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <misc/utils.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

VulkanPipeline::VulkanPipeline(VkDevice device, VkDescriptorSetLayout layout)
{
    VkPipelineLayoutCreateInfo layoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = layout != nullptr ? 1u : 0u,
        .pSetLayouts = &layout
    };

    if (vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create the pipeline layout");
    }
}

void VulkanPipeline::release(VkDevice device)
{
    if (_pipelineLayout) {
        vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
    }

    for (VkShaderModule module : _shaderModules) {
        vkDestroyShaderModule(device, module, nullptr);
    }

    if (_pipeline) {
        vkDestroyPipeline(device, _pipeline, nullptr);
    }
}

bool VulkanPipeline::loadShader(VkDevice device, const std::filesystem::path& path, VkShaderModule& module) const
{
    if (!path.has_filename() || !std::filesystem::exists(path)) {
        NH3D_WARN("Shader at \"" << path << "\" does not exist");
        return false;
    }

    std::ifstream file { path, std::ios::ate | std::ios::binary };
    if (!file.is_open()) {
        NH3D_WARN("Failed to open shader at \"" << path << "\"");
        return false;
    }

    size_t size = file.tellg();
    file.seekg(0);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    file.close();

    VkShaderModuleCreateInfo moduleCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = reinterpret_cast<uint32_t*>(buffer.data())
    };

    return vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &module) == VK_SUCCESS;
}

}