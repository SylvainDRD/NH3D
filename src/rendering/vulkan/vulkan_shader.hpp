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
        const VkFormat format;
        const VkColorComponentFlags colorWriteMask;
        const VkBlendFactor srcColorBlendFactor;
        const VkBlendFactor dstColorBlendFactor;
        const VkBlendOp colorBlendOp;
        const VkBlendFactor srcAlphaBlendFactor;
        const VkBlendFactor dstAlphaBlendFactor;
        const VkBlendOp alphaBlendOp;
        const VkBool32 blendEnable;
    };

    struct VertexInputInfo {
        const ArrayWrapper<VkVertexInputBindingDescription> bindingDescriptions;
        const ArrayWrapper<VkVertexInputAttributeDescription> attributeDescriptions;
    };

    struct ShaderInfo {
        const std::filesystem::path& vertexShaderPath;
        const std::filesystem::path& fragmentShaderPath;
        const VertexInputInfo vertexInputInfo;
        const VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        const VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const ArrayWrapper<ColorAttachmentInfo> colorAttachmentFormats;
        const VkFormat depthAttachmentFormat;
        const VkFormat stencilAttachmentFormat;
        const ArrayWrapper<VkDescriptorSetLayout> descriptorSetsLayouts;
        const ArrayWrapper<VkPushConstantRange> pushConstantRanges;
    };

    /// "Constructor"
    [[nodiscard]] static std::pair<VkPipeline, VkPipelineLayout> create(VkDevice device, const ShaderInfo& shaderInfo);

    // "Destructor": used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout);

    static bool valid(const VkPipeline pipeline, const VkPipelineLayout layout);

    struct DrawParameters {
        const VkBuffer drawIndirectBuffer;
        const VkBuffer drawIndirectCountBuffer;
        const uint32 maxDrawCount;
        const VkExtent2D extent;
        const ArrayWrapper<VkRenderingAttachmentInfo> colorAttachments;
        const VkRenderingAttachmentInfo depthAttachment;
        const VkRenderingAttachmentInfo stencilAttachment;
    };
    static void draw(const VkCommandBuffer commandBuffer, const VkPipeline pipeline, const DrawParameters& params);
};

}