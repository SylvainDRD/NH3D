#include "vulkan_buffer.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

std::pair<GPUBuffer, BufferAllocationInfo> VulkanBuffer::create(const VulkanRHI& rhi, const CreateInfo& info)
{
    // TODO: see if this works out in the future
    const VkBufferUsageFlags updatedUsageFlags = info.usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const VkBufferCreateInfo bufferCreateInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = info.size,
        .usage = updatedUsageFlags,
    };

    const VmaAllocationCreateInfo allocationCreateInfo { .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = info.memoryUsage };

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    if (vmaCreateBuffer(rhi.getAllocator(), &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo)
        != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan buffer creation failed");
    }

    if (info.initialData.ptr != nullptr && info.initialData.size > 0) {
        if (allocationInfo.pMappedData != nullptr) {
            if (info.initialData.size > info.size) {
                NH3D_WARN("Initial data size is larger than buffer size, truncating data copy.");
            }

            std::memcpy(allocationInfo.pMappedData, info.initialData.ptr, std::min(info.initialData.size, info.size));
            if (info.memoryUsage != VMA_MEMORY_USAGE_CPU_ONLY) {
                vmaFlushAllocation(rhi.getAllocator(), allocation, 0, info.size);
            }
        } else {
            // TODO: reuse staging buffers for multiple uploads
            auto [stagingBuffer, stagingAllocation] = VulkanBuffer::create(rhi,
                {
                    .size = info.size,
                    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                    .initialData = info.initialData,
                });

            rhi.executeImmediateCommandBuffer([&info, stagingBuffer, buffer](VkCommandBuffer cmdBuffer) {
                VulkanBuffer::copyBuffer(cmdBuffer, stagingBuffer.buffer, buffer, info.size);
            });

            VulkanBuffer::release(rhi, stagingBuffer, stagingAllocation);
        }
    }

    return { { buffer, info.size }, { allocation, allocationInfo } };
}

void VulkanBuffer::release(const IRHI& rhi, GPUBuffer& buffer, BufferAllocationInfo& allocation)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);

    vmaDestroyBuffer(vrhi.getAllocator(), buffer.buffer, allocation.allocation);
    buffer.buffer = nullptr;
    allocation.allocation = nullptr;
    allocation.allocationInfo = {};
}

bool VulkanBuffer::valid(const GPUBuffer buffer, const BufferAllocationInfo& allocation)
{
    return buffer.buffer != nullptr && allocation.allocation != nullptr;
}

[[nodiscard]] VkDeviceAddress VulkanBuffer::getDeviceAddress(const VulkanRHI& rhi, const VkBuffer buffer)
{
    const VkBufferDeviceAddressInfo deviceAddressInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
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

void VulkanBuffer::copyBuffer(VkCommandBuffer commandBuffer, const VkBuffer srcBuffer, const VkBuffer dstBuffer, const size_t size)
{
    if (size == 0) {
        return;
    }

    const VkBufferCopy copyRegion { .size = size };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
}

void VulkanBuffer::insertMemoryBarrier(VkCommandBuffer commandBuffer, const VkBuffer buffer, const VkAccessFlags2 srcAccessMask,
    const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 dstAccessMask, const VkPipelineStageFlags2 dstStageMask)
{
    const VkBufferMemoryBarrier2 bufferMemoryBarrier {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .buffer = buffer,
        .size = VK_WHOLE_SIZE,
    };

    const VkDependencyInfo dependencyInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferMemoryBarrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

} // namespace NH3D