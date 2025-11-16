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

    std::pair<VkPhysicalDevice, PhysicalDeviceQueueFamilyID> selectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) const;

    VkDevice createLogicalDevice(VkPhysicalDevice gpu, PhysicalDeviceQueueFamilyID queues) const;

    std::pair<VkSwapchainKHR, VkFormat> createSwapchain(VkDevice device, VkPhysicalDevice gpu, VkSurfaceKHR surface,
        PhysicalDeviceQueueFamilyID queues, VkExtent2D extent, VkSwapchainKHR previousSwapchain = nullptr) const;

    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex) const;

    void allocateCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t bufferCount, VkCommandBuffer* buffers) const;

    VkSemaphore createSemaphore(VkDevice device) const;

    VkFence createFence(VkDevice device, bool signaled) const;

    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags, bool resetCommandBuffer = true) const;

    VkSemaphoreSubmitInfo makeSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask) const;

    void submitCommandBuffer(VkQueue queue, const VkSemaphoreSubmitInfo& waitSemaphore, const VkSemaphoreSubmitInfo& signalSemaphore,
        VkCommandBuffer commandBuffer, VkFence fence) const;

    VmaAllocator createVMAAllocator(VkInstance instance, VkPhysicalDevice gpu, VkDevice device) const;

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