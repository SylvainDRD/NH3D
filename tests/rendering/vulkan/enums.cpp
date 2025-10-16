#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <rendering/core/enums.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>

namespace NH3D::Test {

TEST(VulkanEnumTest, BufferUsageFlagsMapping)
{
    EXPECT_EQ(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MapBufferUsageFlagBits(BufferUsageFlagBits::STORAGE_BUFFER_BIT));
    EXPECT_EQ(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MapBufferUsageFlagBits(BufferUsageFlagBits::SRC_TRANSFER_BIT));
    EXPECT_EQ(VK_BUFFER_USAGE_TRANSFER_DST_BIT, MapBufferUsageFlagBits(BufferUsageFlagBits::DST_TRANSFER_BIT));

    // TODO: rewrite to test both individual values and combinations
}

TEST(VulkanEnumTest, BufferMemoryUsageMapping)
{
    constexpr VmaMemoryUsage Expected[] = { VMA_MEMORY_USAGE_GPU_ONLY, VMA_MEMORY_USAGE_CPU_ONLY, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_MEMORY_USAGE_GPU_TO_CPU };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VmaMemoryUsage);
    EXPECT_EQ(TestCases, BufferMemoryUsage::MAX);

    for (uint32_t i = 0; i < std::max(TestCases, static_cast<uint32_t>(BufferMemoryUsage::MAX)); ++i) {
        EXPECT_EQ(MapBufferMemoryUsage(static_cast<BufferMemoryUsage>(i)), Expected[i]);
    }
}

}