#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/rhi.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct GPUBuffer {
    VkBuffer buffer;
};

struct BufferAllocationInfo {
    uint32 allocatedSize; // in bytes, the value returned by VMA allocationInfo.size is unreliable because not all memory is usable safely
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
};

struct VulkanBuffer {
private:
    // 128 MB, see https://docs.vulkan.org/spec/latest/chapters/limits.html
    constexpr static size_t maxGuaranteedStagingBufferSize = 1 << 27;
    static size_t stagingBufferWriteOffset;
    static GPUBuffer stagingBuffer;
    static BufferAllocationInfo stagingBufferAllocation;

public:
    using ResourceType = Buffer;

    using HotType = GPUBuffer;

    using ColdType = BufferAllocationInfo;

    static void flushedUploadCommands() { stagingBufferWriteOffset = 0; }

    struct CreateInfo {
        const uint32 size;
        const VkBufferUsageFlags usage;
        const VmaMemoryUsage memoryUsage;
        const ArrayWrapper<byte> initialData;
    };
    [[nodiscard]]
    static std::pair<GPUBuffer, BufferAllocationInfo> create(VulkanRHI& rhi, const CreateInfo& info);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, GPUBuffer& buffer, BufferAllocationInfo& allocation);

    // Used generically by the ResourceManager, must be API agnostic
    static bool valid(const GPUBuffer buffer, const BufferAllocationInfo& allocation);

    [[nodiscard]] static VkDeviceAddress getDeviceAddress(const VulkanRHI& rhi, const VkBuffer buffer);

    static void flush(const VulkanRHI& rhi, const BufferAllocationInfo& allocation);

    [[nodiscard]] static void* getMappedAddress(const VulkanRHI& rhi, const BufferAllocationInfo& allocation);

    static void copyBuffer(VkCommandBuffer commandBuffer, const VkBuffer srcBuffer, const VkBuffer dstBuffer, const size_t size,
        const size_t srcOffset = 0, const size_t dstOffset = 0);

    static void insertMemoryBarrier(VkCommandBuffer commandBuffer, const VkBuffer buffer, const VkAccessFlags2 srcAccessMask,
        const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 dstAccessMask, const VkPipelineStageFlags2 dstStageMask);
};

}