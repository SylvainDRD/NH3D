#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

struct VulkanShader : public VulkanPipeline {
    using ResourceType = Shader;

struct ShaderInfo {
    std::filesystem::path vertexShaderPath;
    std::filesystem::path fragmentShaderPath;
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthAttachmentFormat;
    VkFormat stencilAttachmentFormat;
};

    /// "Constructor"
    [[nodiscard]] static std::pair<Pipeline, PipelineLayout> create(VkDevice device, const VkDescriptorSetLayout layout,
        const ShaderInfo& shaderInfo, const std::vector<VkPushConstantRange>& pushConstantRanges = {});

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, Pipeline& pipeline, PipelineLayout& pipelineLayout);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const Pipeline pipeline, const PipelineLayout layout)
    {
        return pipeline.pipeline != nullptr && layout.layout != nullptr;
    }

    static void draw(VkCommandBuffer commandBuffer, const Pipeline pipeline, const VkExtent2D extent,
        const std::vector<VkRenderingAttachmentInfo>& colorAttachments, const VkRenderingAttachmentInfo depthAttachment = {},
        const VkRenderingAttachmentInfo stencilAttachment = {});
};

}