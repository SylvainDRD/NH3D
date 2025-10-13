#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"
#include <gtest/gtest.h>
#include <rendering/core/enums.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>

namespace NH3D::Test {

TEST(VulkanEnumTest, BufferUsageFlagsMapping)
{
    EXPECT_EQ(BufferUsageFlags::NH3D_BUFFER_USAGE_MAX, sizeof(_Private::VulkanBufferUsageFlags) / sizeof(_Private::VulkanBufferUsageFlags[0]));

    EXPECT_EQ(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MapBufferUsageFlagBits(BufferUsageFlags::NH3D_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    EXPECT_EQ(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MapBufferUsageFlagBits(BufferUsageFlags::NH3D_BUFFER_USAGE_SRC_TRANSFER_BIT));
    EXPECT_EQ(VK_BUFFER_USAGE_TRANSFER_DST_BIT, MapBufferUsageFlagBits(BufferUsageFlags::NH3D_BUFFER_USAGE_DST_TRANSFER_BIT));
}

TEST(VulkanEnumTest, BufferMemoryUsageMapping)
{
    EXPECT_EQ(BufferMemoryUsage::NH3D_MEM_USAGE_MAX, sizeof(_Private::VulkanMemoryUsageFlags) / sizeof(_Private::VulkanMemoryUsageFlags[0]));

    EXPECT_EQ(VMA_MEMORY_USAGE_GPU_ONLY, MapBufferMemoryUsage(BufferMemoryUsage::NH3D_MEM_USAGE_GPU_ONLY));
    EXPECT_EQ(VMA_MEMORY_USAGE_CPU_ONLY, MapBufferMemoryUsage(BufferMemoryUsage::NH3D_MEM_USAGE_CPU_ONLY));
}

}