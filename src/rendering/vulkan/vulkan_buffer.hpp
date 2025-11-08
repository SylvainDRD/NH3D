#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/rhi.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

// TODO: manage memory property
struct VulkanBuffer {
    // TODO
    using ResourceType = Buffer;

    struct Buffer {
        VkBuffer buffer;
    };
    using Hot = Buffer;

    struct Allocation {
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo; // TODO: check if required
    };
    using Cold = Allocation;

    static std::pair<VulkanBuffer::Buffer, Allocation> create(const VulkanRHI& rhi, const size_t size, const VkBufferUsageFlags usageFlags,
        const VmaMemoryUsage memoryUsage, const void* initialData);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, Buffer& buffer, Allocation& allocation);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const Buffer& buffer, const Allocation& allocation) { return buffer.buffer != nullptr; }

    [[nodiscard]] static VkDeviceAddress getDeviceAddress(const VulkanRHI& rhi, const Buffer& buffer);

    // TODO: mapping & co
};

}