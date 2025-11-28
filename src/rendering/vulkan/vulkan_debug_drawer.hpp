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

// Encapsulate ImGui debug draw and probably other debug draw stuff in the future
class VulkanDebugDrawer {
    NH3D_NO_COPY_MOVE(VulkanDebugDrawer)
public:
    VulkanDebugDrawer() = delete;

    VulkanDebugDrawer(VulkanRHI* const rhi, const VkExtent2D extent, const VkFormat attachmentFormat);

    ~VulkanDebugDrawer();

    void renderDebugUI(VkCommandBuffer commandBuffer, const uint32 frameInFlightId, const Handle<Texture> renderTarget);

private:
    // Reallocates if necessary
    void updateBuffers(const uint32 bufferId);

private:
    VulkanRHI* const _rhi;

    VkSampler _fontSampler;

    Handle<BindGroup> _fontBindGroup = InvalidHandle<BindGroup>;
    Handle<Shader> _uiShader = InvalidHandle<Shader>;

    Handle<Shader> _lineShader = InvalidHandle<Shader>;
    // TODO: figure out descriptors needed

    // Not per frame, because constant and instanced (& scaled!) many times
    Handle<Buffer> _lineVertexBuffer = InvalidHandle<Buffer>;
    Handle<Buffer> _lineIndexBuffer = InvalidHandle<Buffer>;

    FrameResource<Handle<Buffer>> _vertexBuffers;
    FrameResource<Handle<Buffer>> _indexBuffers;
};

}