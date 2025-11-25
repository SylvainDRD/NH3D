#include "vulkan_shader.hpp"

namespace NH3D {

[[nodiscard]] std::pair<VkPipeline, VkPipelineLayout> VulkanShader::create(VkDevice device, const ShaderInfo& shaderInfo)
{
    const VkPipelineLayout pipelineLayout = createPipelineLayout(device, shaderInfo.descriptorSetsLayouts, shaderInfo.pushConstantRanges);

    const VkPipelineVertexInputStateCreateInfo vertexCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = shaderInfo.vertexInputInfo.bindingDescriptions.size,
        .pVertexBindingDescriptions = shaderInfo.vertexInputInfo.bindingDescriptions.ptr,
        .vertexAttributeDescriptionCount = shaderInfo.vertexInputInfo.attributeDescriptions.size,
        .pVertexAttributeDescriptions = shaderInfo.vertexInputInfo.attributeDescriptions.ptr,
    };

    const VkPipelineInputAssemblyStateCreateInfo iasCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRasterizationStateCreateInfo rasterCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = shaderInfo.cullMode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f,
    };

    const VkPipelineViewportStateCreateInfo viewportCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineMultisampleStateCreateInfo msaaCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = shaderInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = shaderInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL, // reverse Z?
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f,
    };

    NH3D_ASSERT(shaderInfo.colorAttachmentFormats.size <= 4, "Vulkan core guarantees only 4 color attachments");
    std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachments {};

    for (uint32 i = 0; i < shaderInfo.colorAttachmentFormats.size; ++i) {
        blendAttachments[i] = VkPipelineColorBlendAttachmentState {
            .blendEnable = shaderInfo.colorAttachmentFormats.ptr[i].blendEnable,
            .srcColorBlendFactor = shaderInfo.colorAttachmentFormats.ptr[i].srcColorBlendFactor,
            .dstColorBlendFactor = shaderInfo.colorAttachmentFormats.ptr[i].dstColorBlendFactor,
            .colorBlendOp = shaderInfo.colorAttachmentFormats.ptr[i].colorBlendOp,
            .srcAlphaBlendFactor = shaderInfo.colorAttachmentFormats.ptr[i].srcAlphaBlendFactor,
            .dstAlphaBlendFactor = shaderInfo.colorAttachmentFormats.ptr[i].dstAlphaBlendFactor,
            .alphaBlendOp = shaderInfo.colorAttachmentFormats.ptr[i].alphaBlendOp,
            .colorWriteMask = shaderInfo.colorAttachmentFormats.ptr[i].colorWriteMask,
        };
    }

    const VkPipelineColorBlendStateCreateInfo blendCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE, // TODO: mmmmh
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = shaderInfo.colorAttachmentFormats.size,
        .pAttachments = blendAttachments.data(),
    };

    const VkDynamicState dynState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    const VkPipelineDynamicStateCreateInfo dynStateCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size(dynState),
        .pDynamicStates = dynState,
    };

    const VkShaderModule vertexShader = loadShaderModule(device, shaderInfo.vertexShaderPath);
    const VkShaderModule fragmentShader = loadShaderModule(device, shaderInfo.fragmentShaderPath);

    const VkPipelineShaderStageCreateInfo vertexStageCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShader,
        .pName = "main",
    };

    const VkPipelineShaderStageCreateInfo fragmentStageCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentShader,
        .pName = "main",
    };

    const VkPipelineShaderStageCreateInfo stagesCI[] = {
        vertexStageCI,
        fragmentStageCI,
    };

    std::array<VkFormat, 4> colorAttachmentFormats {};
    for (uint32 i = 0; i < shaderInfo.colorAttachmentFormats.size; ++i) {
        colorAttachmentFormats[i] = shaderInfo.colorAttachmentFormats.ptr[i].format;
    }

    const VkPipelineRenderingCreateInfo renderCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = shaderInfo.colorAttachmentFormats.size,
        .pColorAttachmentFormats = colorAttachmentFormats.data(),
        .depthAttachmentFormat = shaderInfo.depthAttachmentFormat,
        .stencilAttachmentFormat = shaderInfo.stencilAttachmentFormat,
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderCI,
        .stageCount = 2, // TODO: expand to support more than fragment and vertex shaders
        .pStages = stagesCI,
        .pVertexInputState = &vertexCI,
        .pInputAssemblyState = &iasCI,
        .pViewportState = &viewportCI,
        .pRasterizationState = &rasterCI,
        .pMultisampleState = &msaaCI,
        .pDepthStencilState = &depthStencilCI,
        .pColorBlendState = &blendCI,
        .pDynamicState = &dynStateCI,
        .layout = pipelineLayout,
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    return { pipeline, pipelineLayout };
}

void VulkanShader::release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)
{
    VulkanPipeline::release(rhi, pipeline, pipelineLayout);
}

bool VulkanShader::valid(const VkPipeline pipeline, const VkPipelineLayout layout) { return pipeline != nullptr && layout != nullptr; }

void VulkanShader::draw(VkCommandBuffer commandBuffer, const VkPipeline pipeline, const DrawParameters& params)
{
    const VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D { {}, params.extent },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(params.colorAttachments.size),
        .pColorAttachments = params.colorAttachments.ptr,
        .pDepthAttachment = params.depthAttachment.imageView != nullptr ? &params.depthAttachment : nullptr,
        .pStencilAttachment = params.stencilAttachment.imageView != nullptr ? &params.stencilAttachment : nullptr,
    };

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = float(params.extent.width),
        .height = float(params.extent.height),
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    const VkRect2D scissor = {
        .extent = params.extent,
    };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDrawIndirect(commandBuffer, params.drawIndirectBuffer, 0, 1, sizeof(VkDrawIndirectCommand));

    vkCmdEndRendering(commandBuffer);
}

}
