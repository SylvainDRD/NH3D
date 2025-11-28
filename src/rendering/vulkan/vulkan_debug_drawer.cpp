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

VulkanDebugDrawer::VulkanDebugDrawer(VulkanRHI* const rhi, const VkExtent2D extent, const VkFormat attachmentFormat)
    : _rhi(rhi)
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);
    ImGui::StyleColorsDark();

    io.DisplaySize = ImVec2 { float(extent.width), float(extent.height) }; // TODO: handle resize
    io.DisplayFramebufferScale = ImVec2 { 1.0f, 1.0f };

    byte* fontData;
    int fontTexWidth, fontTexHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &fontTexWidth, &fontTexHeight);
    const uint32 uploadSize = 4 * fontTexWidth * fontTexHeight * sizeof(byte);

    const Handle<Texture> fontTexture = _rhi->createTexture(VulkanTexture::CreateInfo {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = VkExtent3D { static_cast<uint32>(fontTexWidth), static_cast<uint32>(fontTexHeight), 1 },
        .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
        .initialData = ArrayWrapper<byte> {
            .ptr = fontData,
            .size = uploadSize,
        },
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
    _fontBindGroup = _rhi->createBindGroup(VulkanBindGroup::CreateInfo {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingTypes = { bindingTypes, std::size(bindingTypes) },
    });

    const auto& descriptorSets = _rhi->getBindGroupManager().get<DescriptorSets>(_fontBindGroup);
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
        .bindingDescriptions = { uiBindingDescriptions, std::size(uiBindingDescriptions) },
        .attributeDescriptions = { uiAttributeDescriptions, std::size(uiAttributeDescriptions) },
    };

    const VulkanShader::ColorAttachmentInfo colorAttachmentInfo[] = {
        {
            .format = attachmentFormat,
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
    const VkDescriptorSetLayout bindingLayouts[] = {
        _rhi->getBindGroupManager().get<BindGroupMetadata>(_fontBindGroup).layout,
    };
    const VkPushConstantRange pushConstantRange[] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(ImGuiPushConstants),
        },
    };

    auto [uiPipeline, uiPipelineLayout] = VulkanShader::create(_rhi->getVkDevice(),
        VulkanShader::ShaderInfo {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/debug_ui.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/debug_ui.frag.spv",
            .vertexInputInfo = vertexInputInfo,
            .cullMode = VK_CULL_MODE_NONE,
            .colorAttachmentFormats = { colorAttachmentInfo, std::size(colorAttachmentInfo) },
            .descriptorSetsLayouts = { bindingLayouts, std::size(bindingLayouts) },
            .pushConstantRanges = { pushConstantRange, std::size(pushConstantRange) },
        });
    _uiShader = _rhi->getShaderManager().store(std::move(uiPipeline), std::move(uiPipelineLayout));

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        _vertexBuffers[i] = InvalidHandle<Buffer>;
        _indexBuffers[i] = InvalidHandle<Buffer>;
    }

    const VkVertexInputBindingDescription lineBindingDesc {
        .binding = 0,
        .stride = sizeof(vec3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    const VkVertexInputAttributeDescription lineAttributeDesc {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
    };
    auto [linePipeline, linePipelineLayout] = VulkanShader::create(_rhi->getVkDevice(),
        VulkanShader::ShaderInfo {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/debug_line.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/debug_line.frag.spv",
            .vertexInputInfo = VulkanShader::VertexInputInfo {
                .bindingDescriptions = { &lineBindingDesc, 1 },
                .attributeDescriptions = { &lineAttributeDesc, 1 },
            },
            .cullMode = VK_CULL_MODE_NONE,
            .colorAttachmentFormats = { colorAttachmentInfo, std::size(colorAttachmentInfo) },
            .descriptorSetsLayouts = { bindingLayouts, std::size(bindingLayouts) },
            .pushConstantRanges = { pushConstantRange, std::size(pushConstantRange) },
        });
    _lineShader = _rhi->getShaderManager().store(std::move(linePipeline), std::move(linePipelineLayout));

    auto& bufferManager = _rhi->getBufferManager();
    auto [lineVertexBuffer, lineVertexBufferAllocation] = VulkanBuffer::create(*_rhi,
        {
            .size = 100000 * 8 * sizeof(vec3), // 8 vec3 per AABB, 100000 AABBs tops
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _lineVertexBuffer = _rhi->getBufferManager().store(std::move(lineVertexBuffer), std::move(lineVertexBufferAllocation));

    auto [indexBuffer, indexBufferAllocation] = VulkanBuffer::create(*_rhi,
        {
            .size = 100000 * 24 * sizeof(uint8), // 2 indices * 12 edges as uint8 per AABB, 100000 AABBs tops
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _lineIndexBuffer = bufferManager.store(std::move(indexBuffer), std::move(indexBufferAllocation));

    // Culling stuff
    auto [cullingDrawCounterBuffer, cullingDrawCounterAllocation] = VulkanBuffer::create(*_rhi,
        {
            .size = sizeof(uint32),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });

    _cullingDrawCounterBuffer
        = _rhi->getBufferManager().store(std::move(cullingDrawCounterBuffer), std::move(cullingDrawCounterAllocation));

    auto& frameDataDescriptorSets = _bindGroupManager.get<DescriptorSets>(_cullingFrameDataBindGroup);
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_rhi->getVkDevice(), frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingParametersBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingTransformBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingDrawCounterBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
    }

    const VkDescriptorType drawIndirectTypes[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    _drawIndirectCommandBindGroup = createBindGroup({
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .bindingTypes = { drawIndirectTypes, std::size(drawIndirectTypes) },
    });

    auto [drawIndirectBuffer, drawIndirectAllocation] = VulkanBuffer::create(*this,
        {
            .size = MaxObjects * sizeof(VkDrawIndirectCommand),
            .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _drawIndirectBuffer = _bufferManager.store(std::move(drawIndirectBuffer), std::move(drawIndirectAllocation));

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(_gpu, &properties);
    NH3D_ASSERT(drawIndirectAllocation.allocatedSize / sizeof(VkDrawIndirectCommand) < properties.limits.maxDrawIndirectCount,
        "Insufficient max indirect draw count");

    auto& drawIndirectDescriptorSets = _bindGroupManager.get<DescriptorSets>(_drawIndirectCommandBindGroup);
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, drawIndirectDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = drawIndirectBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }
}

VulkanDebugDrawer::~VulkanDebugDrawer()
{
    ImGui::DestroyContext();
    vkDestroySampler(_rhi->getVkDevice(), _fontSampler, nullptr);
    _rhi = nullptr;
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
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bufferManager.get<GPUBuffer>(_vertexBuffers[frameInFlightId]).buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, bufferManager.get<GPUBuffer>(_indexBuffers[frameInFlightId]).buffer, 0,
        VK_INDEX_TYPE_UINT16); // can use 16 bits indices in indexed drawing

    const VkDeviceAddress vertexBufferAddress
        = VulkanBuffer::getDeviceAddress(*_rhi, bufferManager.get<GPUBuffer>(_vertexBuffers[frameInFlightId]).buffer);

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
    if (_vertexBuffers[frameInFlightId] == InvalidHandle<Buffer>
        || bufferManager.get<BufferAllocationInfo>(_vertexBuffers[frameInFlightId]).allocatedSize < vertexBufferSize) {
        if (_vertexBuffers[frameInFlightId] != InvalidHandle<Buffer>) {
            _rhi->destroyBuffer(_vertexBuffers[frameInFlightId]);
        }

        _vertexBuffers[frameInFlightId] = _rhi->createBuffer(Buffer::CreateInfo {
            .size = vertexBufferSize + extraMemory,
            .usageFlags = BufferUsageFlagBits::VERTEX_BUFFER_BIT,
            .memoryUsage = BufferMemoryUsage::CPU_GPU,
        });
    }

    if (_indexBuffers[frameInFlightId] == InvalidHandle<Buffer>
        || bufferManager.get<BufferAllocationInfo>(_indexBuffers[frameInFlightId]).allocatedSize < indexBufferSize) {
        if (_indexBuffers[frameInFlightId] != InvalidHandle<Buffer>) {
            _rhi->destroyBuffer(_indexBuffers[frameInFlightId]);
        }

        _indexBuffers[frameInFlightId] = _rhi->createBuffer(Buffer::CreateInfo {
            .size = indexBufferSize + extraMemory,
            .usageFlags = BufferUsageFlagBits::INDEX_BUFFER_BIT,
            .memoryUsage = BufferMemoryUsage::CPU_GPU,
        });
    }

    // Update data
    const auto& vertexBufferAllocation = bufferManager.get<BufferAllocationInfo>(_vertexBuffers[frameInFlightId]);
    ImDrawVert* vertexDst = reinterpret_cast<ImDrawVert*>(
        VulkanBuffer::getMappedAddress(*_rhi, bufferManager.get<BufferAllocationInfo>(_vertexBuffers[frameInFlightId])));

    const auto& indexBufferAllocation = bufferManager.get<BufferAllocationInfo>(_indexBuffers[frameInFlightId]);
    ImDrawIdx* indexDst = reinterpret_cast<ImDrawIdx*>(
        VulkanBuffer::getMappedAddress(*_rhi, bufferManager.get<BufferAllocationInfo>(_indexBuffers[frameInFlightId])));

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