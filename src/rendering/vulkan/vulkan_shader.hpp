#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

struct ShaderInfo {
    std::filesystem::path vertexShaderPath;
    std::filesystem::path fragmentShaderPath;
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthAttachmentFormat;
    VkFormat stencilAttachmentFormat;
};

struct VulkanShader : public VulkanPipeline {
    using ResourceType = Shader;

    using Hot = VkPipeline;

    using Cold = VkPipelineLayout;

    /// "Constructor"
    [[nodiscard]] static std::pair<VkPipeline, VkPipelineLayout> create(VkDevice device, VkDescriptorSetLayout layout,
        const ShaderInfo& shaderInfo, const std::vector<VkPushConstantRange>& pushConstantRanges = {});

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const VkPipeline pipeline, const VkPipelineLayout layout)
    {
        return pipeline != nullptr && layout != nullptr;
    }

    static inline void draw(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkExtent2D extent,
        const std::vector<VkRenderingAttachmentInfo>& colorAttachments, VkRenderingAttachmentInfo depthAttachment = {},
        VkRenderingAttachmentInfo stencilAttachment = {});
};

}