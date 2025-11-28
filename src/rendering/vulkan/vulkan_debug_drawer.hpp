#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/compute_shader.hpp>
#include <rendering/core/frame_resource.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/core/texture.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct DebugDrawSetupData {
    VkExtent2D extent;
    VkFormat attachmentFormat;
    const FrameResource<Handle<Buffer>>& transformDataBuffers;
    const FrameResource<Handle<Buffer>>& objectAABBsBuffers;
};

// Encapsulate ImGui debug draw and probably other debug draw stuff in the future
class VulkanDebugDrawer {
    NH3D_NO_COPY_MOVE(VulkanDebugDrawer)
public:
    VulkanDebugDrawer() = delete;

    VulkanDebugDrawer(VulkanRHI* const rhi, const DebugDrawSetupData& setupData);

    ~VulkanDebugDrawer();

    void renderAABBs(VkCommandBuffer commandBuffer, const uint32_t frameInFlightId, const mat4& viewMatrix, const mat4& projectionMatrix,
        const uint32 objectCount, const Handle<Texture> depthTexture, const Handle<Texture> renderTarget);

    void renderDebugUI(VkCommandBuffer commandBuffer, const uint32 frameInFlightId, const Handle<Texture> renderTarget);

private:
    // Reallocates if necessary
    void updateBuffers(const uint32 bufferId);

private:
    VulkanRHI* const _rhi;

    VkSampler _fontSampler;

    Handle<BindGroup> _fontBindGroup = InvalidHandle<BindGroup>;
    Handle<Shader> _uiShader = InvalidHandle<Shader>;

    Handle<Shader> _aabbShader = InvalidHandle<Shader>;
    Handle<BindGroup> _objectDataBindGroup = InvalidHandle<BindGroup>;

    // Per-frame buffers
    FrameResource<Handle<Buffer>> _uiVertexBuffers;
    FrameResource<Handle<Buffer>> _uiIndexBuffers;
};

}