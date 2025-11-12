#include "vulkan_shader.hpp"

namespace NH3D {

[[nodiscard]] std::pair<VkPipeline, VkPipelineLayout> VulkanShader::create(
    VkDevice device, VkDescriptorSetLayout layout, const ShaderInfo& shaderInfo, const std::vector<VkPushConstantRange>& pushConstantRanges)
{
    const VkPipelineLayout pipelineLayout = createPipelineLayout(device, layout, pushConstantRanges);

    VkPipelineVertexInputStateCreateInfo vertexCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iasCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE };

    VkPipelineRasterizationStateCreateInfo rasterCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f };

    VkPipelineViewportStateCreateInfo viewportCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1
    };

    VkPipelineMultisampleStateCreateInfo msaaCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE };

    VkPipelineDepthStencilStateCreateInfo depthStencilCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f };

    VkPipelineColorBlendAttachmentState blendAttachment { .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

    VkPipelineColorBlendStateCreateInfo blendCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE, // TODO: mmmmh
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment };

    VkDynamicState dynState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynStateCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(dynState) / sizeof(VkDynamicState),
        .pDynamicStates = dynState };

    const VkShaderModule vertexShader = loadShader(device, shaderInfo.vertexShaderPath);
    const VkShaderModule fragmentShader = loadShader(device, shaderInfo.fragmentShaderPath);

    VkPipelineShaderStageCreateInfo vertexStageCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShader,
        .pName = "main" };

    VkPipelineShaderStageCreateInfo fragmentStageCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentShader,
        .pName = "main" };

    const VkPipelineShaderStageCreateInfo stagesCI[] = { vertexStageCI, fragmentStageCI };

    VkPipelineRenderingCreateInfo renderCI { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = static_cast<uint32_t>(shaderInfo.colorAttachmentFormats.size()),
        .pColorAttachmentFormats = shaderInfo.colorAttachmentFormats.data(),
        .depthAttachmentFormat = shaderInfo.depthAttachmentFormat,
        .stencilAttachmentFormat = shaderInfo.stencilAttachmentFormat };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .layout = pipelineLayout };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    return { pipeline, pipelineLayout };
}

void VulkanShader::draw(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkExtent2D extent,
    const std::vector<VkRenderingAttachmentInfo>& attachments, VkRenderingAttachmentInfo depthAttachment,
    VkRenderingAttachmentInfo stencilAttachment)
{
    VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D { {}, extent },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(attachments.size()),
        .pColorAttachments = attachments.data(),
        .pDepthAttachment = depthAttachment.imageView != nullptr ? &depthAttachment : nullptr,
        .pStencilAttachment = stencilAttachment.imageView != nullptr ? &stencilAttachment : nullptr,
    };

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport
        = { .x = 0, .y = 0, .width = float(extent.width), .height = float(extent.height), .minDepth = 0.f, .maxDepth = 1.f };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .extent = extent,
    };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(commandBuffer);
}

}