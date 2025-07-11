#pragma once

#include <filesystem>
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

class VulkanGraphicsPipeline : public VulkanPipeline {
    NH3D_NO_COPY_MOVE(VulkanGraphicsPipeline)
public:
    VulkanPipeline(VkDevice device, VkDescriptorSetLayout layout, const ShaderData& shaderData);

    void draw(VkCommandBuffer commandBuffer, VkExtent2D extent, const std::vector<VkRenderingAttachmentInfo>& colorAttachments, VkRenderingAttachmentInfo depthAttachment = {}, VkRenderingAttachmentInfo stencilAttachment = {});

public:
    struct ShaderData {
        std::filesystem::path vertexShaderPath;
        std::filesystem::path fragmentShaderPath;
        std::vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat;
        VkFormat stencilAttachmentFormat;
    };
};

}