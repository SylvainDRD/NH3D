#pragma once

#include <array>
#include <cstdint>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/compute_shader.hpp>
#include <rendering/core/handle.hpp>
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

class VulkanRHI : public IRHI {
    NH3D_NO_COPY_MOVE(VulkanRHI)
public:
    VulkanRHI() = delete;

    VulkanRHI(const Window& _window);

    ~VulkanRHI();

    inline VkDevice getVkDevice() const { return _device; }

    inline VmaAllocator getAllocator() const { return _allocator; }

    inline VkCommandBuffer getCommandBuffer() const { return _commandBuffers[_frameId % MaxFramesInFlight]; }

    void executeImmediateCommandBuffer(const std::function<void(VkCommandBuffer)>& recordFunction) const;

    virtual Handle<Texture> createTexture(const Texture::CreateInfo& info) override;

    virtual void destroyTexture(const Handle<Texture> handle) override;

    virtual Handle<Buffer> createBuffer(const Buffer::CreateInfo& info) override;

    virtual void destroyBuffer(const Handle<Buffer> handle) override;

    virtual void render(Scene& scene) const override;

private:
    struct PhysicalDeviceQueueFamilyID {
        uint32_t GraphicsQueueFamilyID = NH3D_MAX_T(uint32_t);
        uint32_t PresentQueueFamilyID = NH3D_MAX_T(uint32_t);

        bool isValid() const { return GraphicsQueueFamilyID != NH3D_MAX_T(uint32_t) && PresentQueueFamilyID != NH3D_MAX_T(uint32_t); }
    };

    Handle<Texture> createTexture(const VulkanTexture::CreateInfo& info);

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

    VkSampler createSampler(const VkDevice device) const;

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
    VkFence _immediateCommandFence;

    std::array<VkCommandBuffer, MaxFramesInFlight> _commandBuffers;
    std::array<VkFence, MaxFramesInFlight> _frameFences;
    std::array<Handle<Texture>, MaxFramesInFlight> _renderTargets;

    std::array<VkSemaphore, MaxFramesInFlight> _presentSemaphores;
    std::vector<VkSemaphore> _renderSemaphores;

    VkSampler _linearSampler;
    mutable ResourceManager<VulkanTexture> _textureManager;
    mutable ResourceManager<VulkanBuffer> _bufferManager;
    mutable ResourceManager<VulkanShader> _shaderManager;
    mutable ResourceManager<VulkanComputeShader> _computeShaderManager;
    mutable ResourceManager<VulkanBindGroup> _bindGroupManager;

    // TODO: replace by a proper render loop
    Handle<Shader> _graphicsShader;
    Handle<ComputeShader> _computeShader;
    Handle<BindGroup> _computeBindGroup;
    Handle<BindGroup> _drawRecordBindGroup;
    Handle<Buffer> _drawIndirectBuffer;
    Handle<Buffer> _drawRecordBuffer;
    Handle<BindGroup> _textureBindGroup;

    mutable uint32_t _frameId = 0;
};

}