#include "vulkan_buffer.hpp"
#include <misc/utils.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

size_t VulkanBuffer::stagingBufferWriteOffset = 0;
GPUBuffer VulkanBuffer::stagingBuffer = { nullptr };
BufferAllocationInfo VulkanBuffer::stagingBufferAllocation;

std::pair<GPUBuffer, BufferAllocationInfo> VulkanBuffer::create(IRHI& rhi, const CreateInfo& info)
{
    VulkanRHI& vrhi = static_cast<VulkanRHI&>(rhi);

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
    if (vmaCreateBuffer(vrhi.getAllocator(), &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo)
        != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan buffer creation failed");
    }

    if (info.initialData.data != nullptr && info.initialData.size > 0) {
        if (allocationInfo.pMappedData != nullptr) {
            if (info.initialData.size > info.size) {
                NH3D_WARN("Initial data size is larger than buffer size, truncating data copy.");
            }

            std::memcpy(allocationInfo.pMappedData, info.initialData.data, std::min(info.initialData.size, info.size));
            if (info.memoryUsage != VMA_MEMORY_USAGE_CPU_ONLY) {
                vmaFlushAllocation(vrhi.getAllocator(), allocation, 0, info.initialData.size);
            }
        } else {
            if (stagingBuffer.buffer == VK_NULL_HANDLE) {
                auto& bufferManager = vrhi.getBufferManager();
                Handle<Buffer> stagingBufferHandle = bufferManager.create(vrhi,
                    {
                        .size = VulkanBuffer::maxGuaranteedStagingBufferSize,
                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                    });

                VulkanBuffer::stagingBuffer = bufferManager.get<GPUBuffer>(stagingBufferHandle);
                VulkanBuffer::stagingBufferAllocation = bufferManager.get<BufferAllocationInfo>(stagingBufferHandle);
            }

            if (info.size > maxGuaranteedStagingBufferSize) {
                NH3D_ABORT("Initial data size is larger than maximum guaranteed staging buffer size.");
            }

            if (stagingBufferWriteOffset + info.initialData.size > VulkanBuffer::maxGuaranteedStagingBufferSize) {
                vrhi.flushUploadCommands();
            }
            void* mappedAddress = VulkanBuffer::stagingBufferAllocation.allocationInfo.pMappedData;
            std::memcpy(static_cast<byte*>(mappedAddress) + stagingBufferWriteOffset, info.initialData.data, info.initialData.size);

            vrhi.recordBufferUploadCommands([&info, buffer](VkCommandBuffer cmdBuffer) {
                VulkanBuffer::copyBuffer(
                    cmdBuffer, VulkanBuffer::stagingBuffer.buffer, buffer, info.initialData.size, stagingBufferWriteOffset);
            });
            stagingBufferWriteOffset += info.initialData.size;
        }
    }

    return { { buffer }, { info.size, allocation, allocationInfo } };
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

void VulkanBuffer::copyBuffer(VkCommandBuffer commandBuffer, const VkBuffer srcBuffer, const VkBuffer dstBuffer, const size_t size,
    const size_t srcOffset, const size_t dstOffset)
{
    if (size == 0) {
        return;
    }

    const VkBufferCopy copyRegion {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size,
    };
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