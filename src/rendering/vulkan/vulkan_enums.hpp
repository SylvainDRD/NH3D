#pragma once

#include "vulkan/vulkan_core.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace NH3D {

enum class VulkanBufferUsageFlags {
    STORAGE_BUFFER_BIT = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    SRC_TRANSFER_BIT = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    DST_TRANSFER_BIT = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
};

enum class VulkanMemoryUsage {
    GPU_ONLY = VMA_MEMORY_USAGE_GPU_ONLY,
    CPU_ONLY = VMA_MEMORY_USAGE_CPU_ONLY
};

}