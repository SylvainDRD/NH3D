#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/rhi.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

    struct BufferAllocationInfo {
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo; // TODO: check if required
    };

// TODO: manage memory property
struct VulkanBuffer {
    using ResourceType = Buffer;

    using HotType = VkBuffer;

    using ColdType = BufferAllocationInfo;

    static std::pair<VkBuffer, BufferAllocationInfo> create(const VulkanRHI& rhi, const size_t size, const VkBufferUsageFlags usageFlags,
        const VmaMemoryUsage memoryUsage, const void* initialData);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, VkBuffer& buffer, BufferAllocationInfo& allocation);

    // Used generically by the ResourceManager, must be API agnostic
    static bool valid(const VkBuffer buffer, const BufferAllocationInfo& allocation);

    [[nodiscard]] static VkDeviceAddress getDeviceAddress(const VulkanRHI& rhi, const VkBuffer buffer);

    // TODO: mapping & co
};

}