#pragma once

#include <filesystem>
#include <misc/utils.hpp>
#include <misc/types.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>

namespace NH3D {

class VulkanGraphicsPipeline : public VulkanPipeline {
    NH3D_NO_COPY_MOVE(VulkanGraphicsPipeline)
public:
    struct ShaderData;

    VulkanGraphicsPipeline(VkDevice device, VkDescriptorSetLayout layout, const ShaderData& shaderData, const std::vector<VkPushConstantRange>& pushConstantRanges = {});

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