#pragma once

#include <misc/utils.hpp>
#include <misc/types.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/rhi.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct VulkanBuffer {
    // TODO
    using ResourceType = Buffer;

    using Hot = VkBuffer;

    struct Allocation {
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo; // TODO: check if required
    };
    using Cold = Allocation;

    static std::pair<VkBuffer, Allocation> create(const VulkanRHI& rhi, uint32_t size, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkBuffer& buffer, Allocation& allocation);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const VkBuffer& buffer, const Allocation& allocation) { return buffer != nullptr; }
    
};

}