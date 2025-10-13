#pragma once

#include "misc/utils.hpp"
#include "rendering/core/enums.hpp"
#include "vulkan/vulkan_core.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace NH3D {

namespace _Private {
    constexpr static VkBufferUsageFlagBits VulkanBufferUsageFlags[] = {
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    constexpr static VmaMemoryUsage VulkanMemoryUsageFlags[] = {
        VMA_MEMORY_USAGE_GPU_ONLY,
        VMA_MEMORY_USAGE_CPU_ONLY
    };
}

inline VkBufferUsageFlagBits MapBufferUsageFlagBits(BufferUsageFlags flag)
{
    NH3D_ASSERT(flag < sizeof(_Private::VulkanBufferUsageFlags) / sizeof(_Private::VulkanBufferUsageFlags[0]), "Invalid BufferUsageFlag")
    return _Private::VulkanBufferUsageFlags[flag];
}

inline VmaMemoryUsage MapBufferMemoryUsage(BufferMemoryUsage flag)
{
    NH3D_ASSERT(flag < sizeof(_Private::VulkanMemoryUsageFlags) / sizeof(_Private::VulkanMemoryUsageFlags[0]), "Invalid VulkanMemoryUsageFlags")
    return _Private::VulkanMemoryUsageFlags[flag];
}

}