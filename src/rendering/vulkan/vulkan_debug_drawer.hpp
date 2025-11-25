#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
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

    VulkanDebugDrawer(VulkanRHI* rhi, const VkExtent2D extent, const VkFormat attachmentFormat);

    ~VulkanDebugDrawer();

    void renderDebugUI(VkCommandBuffer commandBuffer, const uint32 frameInFlightId, const Handle<Texture> renderTarget);

private:
    // Reallocates if necessary
    void updateBuffers(const uint32 bufferId);

private:
    VulkanRHI* _rhi;

    VkSampler _fontSampler;

    Handle<BindGroup> _fontBindGroup = InvalidHandle<BindGroup>;
    Handle<Shader> _fontShader = InvalidHandle<Shader>;

    std::array<Handle<Buffer>, IRHI::MaxFramesInFlight> _vertexBuffers;
    std::array<Handle<Buffer>, IRHI::MaxFramesInFlight> _indexBuffers;
};

}