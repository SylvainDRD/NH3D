#pragma once

#include <misc/utils.hpp>
#include <rendering/core/rhi_interface.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct _VulkanBuffer {
    // TODO
    using ResourceType = Buffer;

    struct Hot {
        VkBuffer _buffer;
    };

    struct Cold {
        VmaAllocation _allocation = nullptr;
        VmaAllocationInfo _allocationInfo {}; // Necessary to keep?
    };
};

class VulkanBuffer {
    NH3D_NO_COPY(VulkanBuffer)
public:
    VulkanBuffer() = delete;

    VulkanBuffer(const VulkanRHI& rhi, uint32_t size, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage);

    VulkanBuffer(VulkanBuffer&& other);

    VulkanBuffer& operator=(VulkanBuffer&& other);

    void release(const IRHI& rhi);

    [[nodiscard]] bool isValid() const { return _buffer != nullptr; }

private:
    VkBuffer _buffer;
    VmaAllocation _allocation = nullptr;
    VmaAllocationInfo _allocationInfo {}; // Necessary to keep?
};

}