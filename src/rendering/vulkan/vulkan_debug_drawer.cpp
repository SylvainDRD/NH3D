#include "vulkan_debug_drawer.hpp"
#include <imgui.h>
#include <rendering/vulkan/vulkan_bind_group.hpp>
#include <rendering/vulkan/vulkan_buffer.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <rendering/vulkan/vulkan_shader.hpp>
#include <rendering/vulkan/vulkan_texture.hpp>

namespace NH3D {

struct ImGuiPushConstants {
    vec2 scale;
    vec2 translate;
};

struct AABBPushConstants {
    mat4 projectionMatrix;
    mat4 viewMatrix;
};

VulkanDebugDrawer::VulkanDebugDrawer(VulkanRHI* const rhi, const DebugDrawSetupData& setupData)
    : _rhi(rhi)
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);
    ImGui::StyleColorsDark();

    io.DisplaySize = ImVec2 { float(setupData.extent.width), float(setupData.extent.height) }; // TODO: handle resize
    io.DisplayFramebufferScale = ImVec2 { 1.0f, 1.0f };

    byte* fontData;
    int fontTexWidth, fontTexHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &fontTexWidth, &fontTexHeight);
    const uint32 uploadSize = 4 * fontTexWidth * fontTexHeight * sizeof(byte);

    auto& textureManager = _rhi->getTextureManager();
    const Handle<Texture> fontTexture = textureManager.create(*_rhi,
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = VkExtent3D { static_cast<uint32>(fontTexWidth), static_cast<uint32>(fontTexHeight), 1 },
            .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            .initialData = { fontData, uploadSize },
        });

    const VkSamplerCreateInfo samplerInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };
    if (vkCreateSampler(_rhi->getVkDevice(), &samplerInfo, nullptr, &_fontSampler) != VK_SUCCESS) {
        NH3D_ABORT("Failed to create ImGui font sampler");
    }

    const VkDescriptorType bindingTypes[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    auto& bindGroupManager = _rhi->getBindGroupManager();
    _fontBindGroup = bindGroupManager.create(*_rhi,
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .bindingTypes = bindingTypes,
        });

    const auto& descriptorSets = bindGroupManager.get<DescriptorSets>(_fontBindGroup);
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        const VkDescriptorImageInfo fontImageInfo {
            .sampler = _fontSampler,
            .imageView = _rhi->getTextureManager().get<ImageView>(fontTexture).view, // Font texture is the first created texture
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VulkanBindGroup::updateDescriptorSet(
            _rhi->getVkDevice(), descriptorSets.sets[i], fontImageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
    }

    const VkVertexInputBindingDescription uiBindingDescriptions[] = {
        {
            .binding = 0,
            .stride = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };
    const VkVertexInputAttributeDescription uiAttributeDescriptions[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, pos),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = offsetof(ImDrawVert, col),
        },
    };

    const VulkanShader::VertexInputInfo vertexInputInfo {
        .bindingDescriptions = uiBindingDescriptions,
        .attributeDescriptions = uiAttributeDescriptions,
    };

    const VulkanShader::ColorAttachmentInfo colorAttachmentInfo[] = {
        {
            .format = setupData.attachmentFormat,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .blendEnable = VK_TRUE,
        },
    };
    const VkDescriptorSetLayout uiBindingLayouts[] = {
        _rhi->getBindGroupManager().get<BindGroupMetadata>(_fontBindGroup).layout,
    };
    const VkPushConstantRange uiPushConstantRange[] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(ImGuiPushConstants),
        },
    };

    auto& shaderManager = _rhi->getShaderManager();
    _uiShader = shaderManager.create(*_rhi,
        {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/debug_ui.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/debug_ui.frag.spv",
            .vertexInputInfo = vertexInputInfo,
            .cullMode = VK_CULL_MODE_NONE,
            .colorAttachmentFormats = colorAttachmentInfo,
            .descriptorSetsLayouts = uiBindingLayouts,
            .pushConstantRanges = uiPushConstantRange,
            .enableDepthWrite = false,
        });

    const VkDescriptorType aabbBindingTypes[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    _objectDataBindGroup = bindGroupManager.create(*_rhi,
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .bindingTypes = aabbBindingTypes,
        });

    const auto& aabbDescriptorSets = _rhi->getBindGroupManager().get<DescriptorSets>(_objectDataBindGroup);
    auto& bufferManager = _rhi->getBufferManager();
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_rhi->getVkDevice(), aabbDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = bufferManager.get<GPUBuffer>(setupData.transformDataBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_rhi->getVkDevice(), aabbDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = bufferManager.get<GPUBuffer>(setupData.objectAABBsBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    }

    const VkDescriptorSetLayout aabbBindingLayouts[] = {
        _rhi->getBindGroupManager().get<BindGroupMetadata>(_objectDataBindGroup).layout,
    };

    const VkPushConstantRange aabbPushConstantRange[] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(AABBPushConstants),
        },
    };

    _aabbShader = shaderManager.create(*_rhi,
        {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/debug_aabb.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/debug_aabb.frag.spv",
            .cullMode = VK_CULL_MODE_NONE,
            .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
            .colorAttachmentFormats = colorAttachmentInfo,
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .descriptorSetsLayouts = aabbBindingLayouts,
            .pushConstantRanges = aabbPushConstantRange,
        });
}

VulkanDebugDrawer::~VulkanDebugDrawer()
{
    ImGui::DestroyContext();
    vkDestroySampler(_rhi->getVkDevice(), _fontSampler, nullptr);
}

void VulkanDebugDrawer::renderAABBs(VkCommandBuffer commandBuffer, const uint32_t frameInFlightId, const mat4& viewMatrix,
    const mat4& projectionMatrix, const uint32 objectCount, const Handle<Texture> depthTexture, const Handle<Texture> renderTarget)
{
    const VkPipeline pipeline = _rhi->getShaderManager().get<VkPipeline>(_aabbShader);

    auto& textureManager = _rhi->getTextureManager();
    const auto& rtImageViewData = textureManager.get<ImageView>(renderTarget);
    const auto& rtMetadata = textureManager.get<TextureMetadata>(renderTarget);

    const VkPipelineLayout pipelineLayout = _rhi->getShaderManager().get<VkPipelineLayout>(_aabbShader);

    const AABBPushConstants pushConstants {
        .projectionMatrix = projectionMatrix,
        .viewMatrix = viewMatrix,
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(AABBPushConstants), &pushConstants);

    auto& bindGroupManager = _rhi->getBindGroupManager();
    const VkDescriptorSet objectDataDescriptorSet = VulkanBindGroup::getUpdatedDescriptorSet(
        _rhi->getVkDevice(), bindGroupManager.get<DescriptorSets>(_objectDataBindGroup), frameInFlightId);
    VulkanBindGroup::bind(commandBuffer, objectDataDescriptorSet, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout);

    VulkanShader::instancedDraw(commandBuffer,
        pipeline,
        {
            .instanceCount = objectCount,
            .indexCount = 24, // 12 lines per AABB
            .drawParams = {
                .extent = { rtMetadata.extent.width, rtMetadata.extent.height },
                .colorAttachments = {
                    {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = rtImageViewData.view,
                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                },
                .depthAttachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = textureManager.get<ImageView>(depthTexture).view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                },
            }
        });
}

void VulkanDebugDrawer::renderDebugUI(VkCommandBuffer commandBuffer, const uint32_t frameInFlightId, const Handle<Texture> renderTarget)
{
    ImDrawData* drawData = ImGui::GetDrawData();

    if (drawData == nullptr || drawData->CmdListsCount == 0) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    updateBuffers(frameInFlightId);

    auto& bindGroupManager = _rhi->getBindGroupManager();
    auto& shaderManager = _rhi->getShaderManager();
    auto& bufferManager = _rhi->getBufferManager();

    const auto pipeline = shaderManager.get<VkPipeline>(_uiShader);
    const auto pipelineLayout = shaderManager.get<VkPipelineLayout>(_uiShader);

    const VkDescriptorSet fontDescriptorSet = VulkanBindGroup::getUpdatedDescriptorSet(
        _rhi->getVkDevice(), bindGroupManager.get<DescriptorSets>(_fontBindGroup), frameInFlightId);
    VulkanBindGroup::bind(commandBuffer, { &fontDescriptorSet, 1 }, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout);

    auto& textureManager = _rhi->getTextureManager();
    const auto& rtImageViewData = textureManager.get<ImageView>(renderTarget);
    const auto& rtMetadata = textureManager.get<TextureMetadata>(renderTarget);
    const VkRenderingAttachmentInfo colorAttachmentsInfo[] = {
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = rtImageViewData.view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
    };
    const VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D { {}, { rtMetadata.extent.width, rtMetadata.extent.height } },
        .layerCount = 1,
        .colorAttachmentCount = std::size(colorAttachmentsInfo),
        .pColorAttachments = colorAttachmentsInfo,
    };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const ImGuiPushConstants pushConstants {
        .scale = vec2 { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y },
        .translate = vec2 { -1.0f, -1.0f },
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ImGuiPushConstants), &pushConstants);

    const VkViewport viewport {
        .x = 0.0f,
        .y = 0.0f,
        .width = io.DisplaySize.x,
        .height = io.DisplaySize.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bufferManager.get<GPUBuffer>(_uiVertexBuffers[frameInFlightId]).buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, bufferManager.get<GPUBuffer>(_uiIndexBuffers[frameInFlightId]).buffer, 0,
        VK_INDEX_TYPE_UINT16); // can use 16 bits indices in indexed drawing

    const VkDeviceAddress vertexBufferAddress
        = VulkanBuffer::getDeviceAddress(*_rhi, bufferManager.get<GPUBuffer>(_uiVertexBuffers[frameInFlightId]).buffer);

    int32 vertexOffset = 0;
    int32 indexOffset = 0;
    for (uint32 i = 0; i < drawData->CmdListsCount; ++i) {
        const ImDrawList* drawList = drawData->CmdLists[i];

        for (uint32 j = 0; j < drawList->CmdBuffer.Size; ++j) {
            const ImDrawCmd* drawCmd = &drawList->CmdBuffer[j];

            const VkRect2D scissorRect {
                .offset = VkOffset2D {
                    .x = static_cast<int32>(drawCmd->ClipRect.x),
                    .y = static_cast<int32>(drawCmd->ClipRect.y),
                },
                .extent = VkExtent2D {
                    .width = static_cast<uint32>(drawCmd->ClipRect.z - drawCmd->ClipRect.x),
                    .height = static_cast<uint32>(drawCmd->ClipRect.w - drawCmd->ClipRect.y),
                },
            };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);

            vkCmdDrawIndexed(commandBuffer, drawCmd->ElemCount, 1, indexOffset, vertexOffset, 0);

            indexOffset += drawCmd->ElemCount;
        }
        vertexOffset += drawList->VtxBuffer.Size;
    }

    vkCmdEndRendering(commandBuffer);
}

void VulkanDebugDrawer::updateBuffers(const uint32 frameInFlightId)
{
    const ImDrawData* drawData = ImGui::GetDrawData();
    const uint32 vertexBufferSize = static_cast<uint32>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
    const uint32 indexBufferSize = static_cast<uint32>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);

    if (vertexBufferSize == 0 || indexBufferSize == 0) {
        return;
    }

    auto& bufferManager = _rhi->getBufferManager();
    constexpr uint32 extraMemory = 16384;

    // Note to self: VMA extra allocated memory is not safely usable
    if (_uiVertexBuffers[frameInFlightId] == InvalidHandle<Buffer>
        || bufferManager.get<BufferAllocationInfo>(_uiVertexBuffers[frameInFlightId]).allocatedSize < vertexBufferSize) {
        if (_uiVertexBuffers[frameInFlightId] != InvalidHandle<Buffer>) {
            _rhi->destroyBuffer(_uiVertexBuffers[frameInFlightId]);
        }

        _uiVertexBuffers[frameInFlightId] = _rhi->createBuffer(Buffer::CreateInfo {
            .size = vertexBufferSize + extraMemory,
            .usageFlags = BufferUsageFlagBits::VERTEX_BUFFER_BIT,
            .memoryUsage = BufferMemoryUsage::CPU_GPU,
        });
    }

    if (_uiIndexBuffers[frameInFlightId] == InvalidHandle<Buffer>
        || bufferManager.get<BufferAllocationInfo>(_uiIndexBuffers[frameInFlightId]).allocatedSize < indexBufferSize) {
        if (_uiIndexBuffers[frameInFlightId] != InvalidHandle<Buffer>) {
            _rhi->destroyBuffer(_uiIndexBuffers[frameInFlightId]);
        }

        _uiIndexBuffers[frameInFlightId] = _rhi->createBuffer(Buffer::CreateInfo {
            .size = indexBufferSize + extraMemory,
            .usageFlags = BufferUsageFlagBits::INDEX_BUFFER_BIT,
            .memoryUsage = BufferMemoryUsage::CPU_GPU,
        });
    }

    // Update data
    const auto& vertexBufferAllocation = bufferManager.get<BufferAllocationInfo>(_uiVertexBuffers[frameInFlightId]);
    ImDrawVert* vertexDst = reinterpret_cast<ImDrawVert*>(
        VulkanBuffer::getMappedAddress(*_rhi, bufferManager.get<BufferAllocationInfo>(_uiVertexBuffers[frameInFlightId])));

    const auto& indexBufferAllocation = bufferManager.get<BufferAllocationInfo>(_uiIndexBuffers[frameInFlightId]);
    ImDrawIdx* indexDst = reinterpret_cast<ImDrawIdx*>(
        VulkanBuffer::getMappedAddress(*_rhi, bufferManager.get<BufferAllocationInfo>(_uiIndexBuffers[frameInFlightId])));

    for (int i = 0; i < drawData->CmdListsCount; ++i) {
        const ImDrawList* drawList = drawData->CmdLists[i];

        std::memcpy(vertexDst, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy(indexDst, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vertexDst += drawList->VtxBuffer.Size;
        indexDst += drawList->IdxBuffer.Size;
    }

    VulkanBuffer::flush(*_rhi, vertexBufferAllocation);
    VulkanBuffer::flush(*_rhi, indexBufferAllocation);
}

} // namespace NH3D