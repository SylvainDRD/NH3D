#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

struct VulkanShader : public VulkanPipeline {
    using ResourceType = Shader;

    struct ColorAttachmentInfo {
        VkFormat format;
        VkColorComponentFlags colorWriteMask;
        bool blendEnable;
    };

    struct ShaderInfo {
        const std::filesystem::path& vertexShaderPath;
        const std::filesystem::path& fragmentShaderPath;
        ArrayWrapper<ColorAttachmentInfo> colorAttachmentFormats;
        VkFormat depthAttachmentFormat;
        VkFormat stencilAttachmentFormat;
        ArrayWrapper<VkDescriptorSetLayout> descriptorSetsLayouts;
        ArrayWrapper<VkPushConstantRange> pushConstantRanges;
    };

    /// "Constructor"
    [[nodiscard]] static std::pair<VkPipeline, VkPipelineLayout> create(VkDevice device, const ShaderInfo& shaderInfo);

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout);

    static bool valid(const VkPipeline pipeline, const VkPipelineLayout layout);

    struct DrawParameters {
        const VkBuffer drawIndirectBuffer;
        const VkExtent2D extent;
        const ArrayWrapper<VkRenderingAttachmentInfo> colorAttachments;
        const VkRenderingAttachmentInfo depthAttachment;
        const VkRenderingAttachmentInfo stencilAttachment;
    };
    static void draw(const VkCommandBuffer commandBuffer, const VkPipeline pipeline, const DrawParameters& params);
};

}