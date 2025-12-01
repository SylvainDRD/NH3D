#pragma once

#include <core/aabb.hpp>
#include <cstdint>
#include <functional>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/compute_shader.hpp>
#include <rendering/core/frame_resource.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/material.hpp>
#include <rendering/core/resource_manager.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/core/texture.hpp>
#include <rendering/vulkan/vulkan_bind_group.hpp>
#include <rendering/vulkan/vulkan_buffer.hpp>
#include <rendering/vulkan/vulkan_compute_shader.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>
#include <rendering/vulkan/vulkan_shader.hpp>
#include <rendering/vulkan/vulkan_texture.hpp>
#include <utility>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanDebugDrawer;

class VulkanRHI : public IRHI {
    NH3D_NO_COPY_MOVE(VulkanRHI)
public:
    VulkanRHI() = delete;

    VulkanRHI(const Window& _window);

    ~VulkanRHI();

    [[nodiscard]] inline VkDevice getVkDevice() const { return _device; }

    [[nodiscard]] inline VmaAllocator getAllocator() const { return _allocator; }

    [[nodiscard]] inline ResourceManager<VulkanTexture>& getTextureManager() { return _textureManager; }

    [[nodiscard]] inline ResourceManager<VulkanBuffer>& getBufferManager() { return _bufferManager; }

    [[nodiscard]] inline ResourceManager<VulkanShader>& getShaderManager() { return _shaderManager; }

    [[nodiscard]] inline ResourceManager<VulkanComputeShader>& getComputeShaderManager() { return _computeShaderManager; }

    [[nodiscard]] inline ResourceManager<VulkanBindGroup>& getBindGroupManager() { return _bindGroupManager; }

    void recordBufferUploadCommands(const std::function<void(VkCommandBuffer)>& recordFunction) const;

    void flushUploadCommands() const;

    void executeImmediateCommandBuffer(const std::function<void(VkCommandBuffer)>& recordFunction) const;

    virtual Handle<Texture> createTexture(const Texture::CreateInfo& info) override;

    Handle<Texture> createTexture(const VulkanTexture::CreateInfo& info);

    virtual void destroyTexture(const Handle<Texture> handle) override;

    virtual Handle<Buffer> createBuffer(const Buffer::CreateInfo& info) override;

    virtual void destroyBuffer(const Handle<Buffer> handle) override;

    virtual void render(Scene& scene) const override;

    Handle<BindGroup> createBindGroup(const VulkanBindGroup::CreateInfo& info);

private:
    struct PhysicalDeviceQueueFamilyID {
        uint32_t GraphicsQueueFamilyID = NH3D_MAX_T(uint32_t);
        uint32_t PresentQueueFamilyID = NH3D_MAX_T(uint32_t);

        bool isValid() const { return GraphicsQueueFamilyID != NH3D_MAX_T(uint32_t) && PresentQueueFamilyID != NH3D_MAX_T(uint32_t); }
    };

    struct MeshData {
        VkDeviceAddress vertexBuffer;
        VkDeviceAddress indexBuffer;
        Material material;
    };

    struct RenderData {
        MeshData mesh;
        uint indexCount;
    };

    struct FrustumPlanes {
        vec2 left;
        vec2 right;
        vec2 bottom;
        vec2 top;
    };

    struct CullingParameters {
        mat4 viewMatrix;
        FrustumPlanes frustumPlanes; // small optimization: consider infinite far plane and assume d=0 for near plane
        uint objectCount;
    };

private:
    VkInstance createVkInstance(std::vector<const char*>&& requiredWindowExtensions) const;

    VkDebugUtilsMessengerEXT createDebugMessenger(const VkInstance instance) const;

    std::pair<VkPhysicalDevice, PhysicalDeviceQueueFamilyID> selectPhysicalDevice(
        const VkInstance instance, const VkSurfaceKHR surface) const;

    VkDevice createLogicalDevice(const VkPhysicalDevice gpu, const PhysicalDeviceQueueFamilyID queues) const;

    std::pair<VkSwapchainKHR, VkFormat> createSwapchain(const VkDevice device, const VkPhysicalDevice gpu, const VkSurfaceKHR surface,
        const PhysicalDeviceQueueFamilyID queues, const VkExtent2D extent, const VkSwapchainKHR previousSwapchain = nullptr) const;

    VkCommandPool createCommandPool(const VkDevice device, const uint32_t queueFamilyIndex) const;

    void allocateCommandBuffers(
        const VkDevice device, const VkCommandPool commandPool, const uint32_t bufferCount, VkCommandBuffer* buffers) const;

    VkSemaphore createSemaphore(const VkDevice device) const;

    VkFence createFence(const VkDevice device, const bool signaled) const;

    void beginCommandBuffer(
        const VkCommandBuffer commandBuffer, const VkCommandBufferUsageFlags flags, const bool resetCommandBuffer = true) const;

    VkSemaphoreSubmitInfo makeSemaphoreSubmitInfo(const VkSemaphore semaphore, const VkPipelineStageFlags2 stageMask) const;

    void submitCommandBuffer(const VkQueue queue, const VkSemaphoreSubmitInfo& waitSemaphore, const VkSemaphoreSubmitInfo& signalSemaphore,
        const VkCommandBuffer commandBuffer, const VkFence fence) const;

    VmaAllocator createVMAAllocator(const VkInstance instance, const VkPhysicalDevice gpu, const VkDevice device) const;

    VkSampler createSampler(const VkDevice device, const bool linear) const;

    FrustumPlanes getFrustumPlanes(const mat4& projectionMatrix) const;

private:
    VkInstance _instance;
#if NH3D_DEBUG
    VkDebugUtilsMessengerEXT _debugUtilsMessenger;
#endif
    VkPhysicalDevice _gpu;

    VkSurfaceKHR _surface;

    VkDevice _device;
    VmaAllocator _allocator;

    VkQueue _graphicsQueue;
    VkQueue _presentQueue;

    VkSwapchainKHR _swapchain;
    std::vector<Handle<Texture>> _swapchainTextures;

    VkCommandPool _commandPool;
    VkCommandPool _immediateCommandPool;
    VkCommandBuffer _immediateCommandBuffer;
    VkCommandPool _uploadCommandPool;
    VkCommandBuffer _uploadCommandBuffer;
    mutable bool _uploadsToBeFlushed = false;
    VkFence _immediateAndUploadFence; // Used for immediate command buffer submission & beginning of frame uploads

    FrameResource<VkCommandBuffer> _commandBuffers;
    FrameResource<VkFence> _frameFences;
    FrameResource<VkSemaphore> _presentSemaphores;
    std::vector<VkSemaphore> _renderSemaphores;

    VkSampler _linearSampler;
    VkSampler _nearestSampler;
    mutable ResourceManager<VulkanTexture> _textureManager;
    mutable ResourceManager<VulkanBuffer> _bufferManager;
    mutable ResourceManager<VulkanShader> _shaderManager;
    mutable ResourceManager<VulkanComputeShader> _computeShaderManager;
    mutable ResourceManager<VulkanBindGroup> _bindGroupManager;
    Handle<ComputeShader> _frustumCullingCS = InvalidHandle<ComputeShader>;
    // Vertices/Indices/Material/AABB + visible flags
    Handle<BindGroup> _cullingRenderDataBindGroup = InvalidHandle<BindGroup>;
    FrameResource<Handle<Buffer>> _cullingRenderDataBuffers = {};
    FrameResource<Handle<Buffer>> _cullingRenderDataStagingBuffers = {};
    FrameResource<Handle<Buffer>> _cullingVisibleFlagBuffers = {};
    FrameResource<Handle<Buffer>> _cullingAABBsBuffers = {};
    FrameResource<Handle<Buffer>> _cullingVisibleFlagStagingBuffers = {};
    Handle<BindGroup> _cullingFrameDataBindGroup = InvalidHandle<BindGroup>;
    FrameResource<Handle<Buffer>> _cullingParametersBuffers = {}; // Update via vkCmdUpdateBuffer
    FrameResource<Handle<Buffer>> _cullingTransformBuffers = {};
    FrameResource<Handle<Buffer>> _cullingTransformStagingBuffers = {};
    FrameResource<Handle<Buffer>> _cullingDrawCounterBuffers = {}; // vkCmdFillBuffer

    struct GBuffer {
        Handle<Texture> normalRT;
        Handle<Texture> albedoRT;
        Handle<Texture> depthRT;
    };
    FrameResource<GBuffer> _gbufferRTs;

    Handle<Shader> _gbufferShader;
    Handle<BindGroup> _drawIndirectCommandBindGroup;
    FrameResource<Handle<Buffer>> _drawIndirectBuffers; // GPU written
    Handle<BindGroup> _drawRecordBindGroup;
    FrameResource<Handle<Buffer>> _drawRecordBuffers; // GPU written
    Handle<BindGroup> _albedoTextureBindGroup;

    Handle<ComputeShader> _deferredShadingCS;
    Handle<BindGroup> _deferredShadingBindGroup;
    FrameResource<Handle<Texture>> _finalRTs;

    Uptr<VulkanDebugDrawer> _debugDrawer;

    mutable uint32_t _frameId = 0;
};

}
