#include "vulkan_buffer.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

std::pair<VkBuffer, VulkanBuffer::Allocation> VulkanBuffer::create(const VulkanRHI& rhi, size_t size, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferCreateInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usageFlags
    };

    VmaAllocationCreateInfo allocationCreateInfo {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memoryUsage
    };

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(rhi.getAllocator(), &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan buffer creation failed");
    }

    return { buffer, { allocation, allocationInfo } };
}

void VulkanBuffer::release(const IRHI& rhi, VkBuffer& buffer, Allocation& allocation)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);

    vmaDestroyBuffer(vrhi.getAllocator(), buffer, allocation.allocation);
}

}