#include "vulkan_buffer.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

VulkanBuffer::VulkanBuffer(VmaAllocator allocator, uint32_t size, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage)
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

    if (vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &_buffer, &_allocation, &_allocationInfo) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan buffer creation failed");
    }
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) {
    _buffer = other._buffer;
    other._buffer = nullptr;
    _allocation = other._allocation;
    other._allocation = nullptr;
    _allocationInfo = other._allocationInfo;
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) {
    _buffer = other._buffer;
    other._buffer = nullptr;
    _allocation = other._allocation;
    other._allocation = nullptr;
    _allocationInfo = other._allocationInfo;

    return *this;
}

void VulkanBuffer::release(const IRHI& rhi) {
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);

    vmaDestroyBuffer(vrhi.getAllocator(), _buffer, _allocation);
}

}