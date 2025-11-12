#pragma once

#include <filesystem>
#include <fstream>
#include <misc/utils.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace NH3D {

struct VulkanPipeline {
    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static inline void VulkanPipeline::release(const IRHI& rhi, VkPipelineLayout& pipelineLayout, VkPipeline& pipeline)
    {
        const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
        if (pipelineLayout) {
            vkDestroyPipelineLayout(vrhi.getVkDevice(), pipelineLayout, nullptr);
            pipelineLayout = nullptr;
        }

        // // TODO: destroy after pipeline creation
        // for (VkShaderModule module : _shaderModules) {
        //     vkDestroyShaderModule(device, module, nullptr);
        // }

        if (pipeline) {
            vkDestroyPipeline(vrhi.getVkDevice(), pipeline, nullptr);
            pipeline = nullptr;
        }
    }

protected:
    static inline VkPipelineLayout createPipelineLayout(
        VkDevice device, VkDescriptorSetLayout layout, const std::vector<VkPushConstantRange>& pushConstantRanges)
    {
        VkPipelineLayoutCreateInfo layoutCreateInfo { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = layout != nullptr ? 1u : 0u,
            .pSetLayouts = &layout,
            .pPushConstantRanges = pushConstantRanges.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()) };

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            NH3D_ABORT_VK("Failed to create the pipeline layout");
        }

        return pipelineLayout;
    }

    static inline VkShaderModule loadShader(VkDevice device, const std::filesystem::path& path)
    {
        if (!path.has_filename() || !std::filesystem::exists(path)) {
            NH3D_ABORT("Shader at \"" << path << "\" does not exist");
        }

        std::ifstream file { path, std::ios::ate | std::ios::binary };
        if (!file.is_open()) {
            NH3D_ABORT("Failed to open shader at \"" << path << "\"");
        }

        size_t size = file.tellg();
        file.seekg(0);

        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        file.close();

        VkShaderModuleCreateInfo moduleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = reinterpret_cast<uint32_t*>(buffer.data())
        };

        VkShaderModule shader;
        if (vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shader) != VK_SUCCESS) {
            NH3D_ABORT("Failed to create shader module from \"" << path << "\"");
        }

        return shader;
    }
};

}