#pragma once

// Resources have to be included before the resource manager, otherwise clang will complain
#include "rendering/vulkan/vulkan_buffer.hpp"
#include "rendering/vulkan/vulkan_texture.hpp"

#include <array>
#include <cstdint>
#include <misc/utils.hpp>
#include <rendering/core/resource_manager.hpp>
#include <rendering/core/rhi_interface.hpp>
#include <rendering/render_graph/render_graph.hpp>
#include <rendering/vulkan/vulkan_buffer.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>
#include <rendering/vulkan/vulkan_texture.hpp>
#include <utility>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class Window;
template <uint32_t>
class VulkanDescriptorSetPool;
class VulkanComputePipeline;
class VulkanGraphicsPipeline;

class VulkanRHI : public IRHI {
    NH3D_NO_COPY_MOVE(VulkanRHI)
public:
    VulkanRHI() = delete;

    VulkanRHI(const Window& _window);

    ~VulkanRHI();

    inline VkDevice getVkDevice() const { return _device; }

    inline VmaAllocator getAllocator() const { return _allocator; }

    inline VkCommandBuffer getCommandBuffer() const { return _commandBuffers[_frameId % MaxFramesInFlight]; }

    virtual void render(const RenderGraph& rdag) const override;

private:
    struct PhysicalDeviceQueueFamilyID {
        uint32_t GraphicsQueueFamilyID = NH3D_MAX_T(uint32_t);
        uint32_t PresentQueueFamilyID = NH3D_MAX_T(uint32_t);

        bool isValid() const { return GraphicsQueueFamilyID != NH3D_MAX_T(uint32_t) && PresentQueueFamilyID != NH3D_MAX_T(uint32_t); }
    };

    VkInstance createVkInstance(std::vector<const char*>&& requiredWindowExtensions) const;

    VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance) const;

    std::pair<VkPhysicalDevice, PhysicalDeviceQueueFamilyID> selectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) const;

    VkDevice createLogicalDevice(VkPhysicalDevice gpu, PhysicalDeviceQueueFamilyID queues) const;

    std::pair<VkSwapchainKHR, VkFormat> createSwapchain(VkDevice device, VkPhysicalDevice gpu, VkSurfaceKHR surface, PhysicalDeviceQueueFamilyID queues, VkExtent2D extent, VkSwapchainKHR previousSwapchain = nullptr) const;

    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex) const;

    void allocateCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t bufferCount, VkCommandBuffer* buffers) const;

    VkSemaphore createSemaphore(VkDevice device) const;

    VkFence createFence(VkDevice device) const;

    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags, bool resetCommandBuffer = true) const;

    VkSemaphoreSubmitInfo makeSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask) const;

    void submitCommandBuffer(VkQueue queue, const VkSemaphoreSubmitInfo& waitSemaphore, const VkSemaphoreSubmitInfo& signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const;

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
    std::vector<VulkanTexture*> _swapchainTextures;

    VkCommandPool _commandPool;

    static constexpr uint32_t MaxFramesInFlight = 2;
    std::array<VkCommandBuffer, MaxFramesInFlight> _commandBuffers;
    std::array<VkFence, MaxFramesInFlight> _frameFences;
    std::array<VulkanTexture*, MaxFramesInFlight> _renderTargets;

    std::array<VkSemaphore, MaxFramesInFlight> _presentSemaphores;
    std::vector<VkSemaphore> _renderSemaphores;

    Uptr<VulkanDescriptorSetPool<MaxFramesInFlight>> _descriptorSetPoolCompute = nullptr;

    ResourceManager _resourceManager;

    Uptr<VulkanComputePipeline> _computePipeline = nullptr;
    Uptr<VulkanGraphicsPipeline> _graphicsPipeline = nullptr;

    mutable uint32_t _frameId = 1;
};

}