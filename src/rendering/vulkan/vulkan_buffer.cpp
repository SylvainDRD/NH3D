#include "vulkan_buffer.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

std::pair<VkBuffer, BufferAllocationInfo> VulkanBuffer::create(
    const VulkanRHI& rhi, const size_t size, const VkBufferUsageFlags usageFlags, const VmaMemoryUsage memoryUsage, const void* initialData)
{
    // TODO: see if this works out in the future
    const VkBufferUsageFlags updatedUsageFlags = usageFlags | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const VkBufferCreateInfo bufferCreateInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = updatedUsageFlags };

    const VmaAllocationCreateInfo allocationCreateInfo { .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = memoryUsage };

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(rhi.getAllocator(), &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo)
        != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan buffer creation failed");
    }

    if (initialData) {
        if (allocationInfo.pMappedData) {
            std::memcpy(allocationInfo.pMappedData, initialData, size);
            if (memoryUsage != VMA_MEMORY_USAGE_CPU_ONLY) {
                vmaFlushAllocation(rhi.getAllocator(), allocation, 0, size);
            }
        } else {
            // TODO: reuse staging buffers for multiple uploads
            auto [stagingBuffer, stagingAllocation]
                = VulkanBuffer::create(rhi, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, initialData);

            rhi.executeImmediateCommandBuffer([size, stagingBuffer, buffer](VkCommandBuffer cmdBuffer) {
                // Copy from staging buffer to the actual buffer
                VkBufferCopy copyRegion { .size = size };
                vkCmdCopyBuffer(cmdBuffer, stagingBuffer, buffer, 1, &copyRegion);
            });

            VulkanBuffer::release(rhi, stagingBuffer, stagingAllocation);
        }
    }

    return { buffer, { allocation, allocationInfo } };
}

void VulkanBuffer::release(const IRHI& rhi, VkBuffer& buffer, BufferAllocationInfo& allocation)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);

    vmaDestroyBuffer(vrhi.getAllocator(), buffer, allocation.allocation);
    buffer = nullptr;
    allocation.allocation = nullptr;
    allocation.allocationInfo = {};
}

bool VulkanBuffer::valid(const VkBuffer buffer, const BufferAllocationInfo& allocation)
{
    return buffer != nullptr && allocation.allocation != nullptr;
}

[[nodiscard]] VkDeviceAddress VulkanBuffer::getDeviceAddress(const VulkanRHI& rhi, const VkBuffer buffer)
{
    VkBufferDeviceAddressInfo deviceAddressInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    return vkGetBufferDeviceAddress(rhi.getVkDevice(), &deviceAddressInfo);
}

void VulkanBuffer::flush(const VulkanRHI& rhi, const BufferAllocationInfo& allocation)
{
    vmaFlushAllocation(rhi.getAllocator(), allocation.allocation, 0, allocation.allocationInfo.size);
}

[[nodiscard]] void* VulkanBuffer::getMappedAddress(const VulkanRHI& rhi, const BufferAllocationInfo& allocation)
{
    return allocation.allocationInfo.pMappedData;
}

} // namespace NH3D