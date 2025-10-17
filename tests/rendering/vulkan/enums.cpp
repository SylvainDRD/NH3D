#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <rendering/core/enums.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>

namespace NH3D::Test {

TEST(VulkanEnumTest, TextureFormatMapping)
{
    // LLM generated, at least they can do that
    constexpr VkFormat Expected[] = {
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R8G8B8_UINT,
        VK_FORMAT_R8G8B8_SINT,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R16G16B16_UINT,
        VK_FORMAT_R16G16B16_SINT,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R32G32B32_SINT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VkFormat);
    EXPECT_EQ(TestCases, static_cast<uint32_t>(TextureFormat::NH3D_TEXTURE_FORMAT_MAX));

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapTextureFormat(static_cast<TextureFormat>(i)), Expected[i]);
    }
}

TEST(VulkanEnumTest, TextureAspectFlagsMapping)
{
    constexpr VkImageAspectFlags Expected[] = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_ASPECT_STENCIL_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VkImageAspectFlags);
    EXPECT_EQ(1 << TestCases, static_cast<uint32_t>(TextureAspectFlagBits::NH3D_TEXTURE_ASPECT_MAX));

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapTextureAspectFlags(static_cast<TextureAspectFlags>(i)), Expected[i]);
    }
}

TEST(VulkanEnumTest, CombinedTextureAspectFlagsMapping)
{
    constexpr VkBufferUsageFlags Expected[] = {
        VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(TextureAspectFlags);

    constexpr TextureAspectFlags Flags[] = {
        TextureAspectFlagBits::ASPECT_COLOR_BIT | TextureAspectFlagBits::ASPECT_STENCIL_BIT,
        TextureAspectFlagBits::ASPECT_DEPTH_BIT | TextureAspectFlagBits::ASPECT_STENCIL_BIT,
        TextureAspectFlagBits::ASPECT_COLOR_BIT | TextureAspectFlagBits::ASPECT_DEPTH_BIT | TextureAspectFlagBits::ASPECT_STENCIL_BIT
    };

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapTextureAspectFlags(Flags[i]), Expected[i]);
    }
}
TEST(VulkanEnumTest, TextureUsageFlagsMapping)
{
    constexpr VkImageUsageFlags Expected[] = {
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VkImageUsageFlags);
    EXPECT_EQ(1 << TestCases, static_cast<uint32_t>(TextureUsageFlagBits::NH3D_TEXTURE_USAGE_MAX));

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapTextureUsageFlags(static_cast<TextureUsageFlags>(i)), Expected[i]);
    }
}

TEST(VulkanEnumTest, CombinedTextureUsageFlagsMapping)
{
    constexpr VkImageUsageFlags Expected[] = {
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(TextureAspectFlags);

    constexpr TextureUsageFlags Flags[] = {
        TextureUsageFlagBits::USAGE_COLOR_BIT | TextureUsageFlagBits::USAGE_STORAGE_BIT,
        TextureUsageFlagBits::USAGE_SAMPLED_BIT | TextureUsageFlagBits::USAGE_INPUT_BIT,
        TextureUsageFlagBits::USAGE_COLOR_BIT | TextureUsageFlagBits::USAGE_DEPTH_STENCIL_BIT | TextureUsageFlagBits::USAGE_SAMPLED_BIT,
        TextureUsageFlagBits::USAGE_COLOR_BIT | TextureUsageFlagBits::USAGE_DEPTH_STENCIL_BIT | TextureUsageFlagBits::USAGE_STORAGE_BIT | TextureUsageFlagBits::USAGE_SAMPLED_BIT | TextureUsageFlagBits::USAGE_INPUT_BIT
    };

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapTextureUsageFlags(Flags[i]), Expected[i]);
    }
}

TEST(VulkanEnumTest, BufferUsageFlagsMapping)
{
    constexpr VkBufferUsageFlags Expected[] = {
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VkBufferUsageFlags);
    EXPECT_EQ(1 << TestCases, static_cast<uint32_t>(BufferUsageFlagBits::NH3D_BUFFER_USAGE_MAX));

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapBufferUsageFlags(static_cast<BufferUsageFlags>(i)), Expected[i]);
    }
}

TEST(VulkanEnumTest, CombinedBufferUsageFlagsMapping)
{
    constexpr VkBufferUsageFlags Expected[] = {
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VkBufferUsageFlags);

    constexpr BufferUsageFlags Flags[] = {
        BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::SRC_TRANSFER_BIT,
        BufferUsageFlagBits::SRC_TRANSFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
        BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::SRC_TRANSFER_BIT | BufferUsageFlagBits::NH3D_BUFFER_USAGE_DST_TRANSFER_BIT
    };

    for (uint32_t i = 0; i < TestCases; ++i) {
        EXPECT_EQ(MapBufferUsageFlags(Flags[i]), Expected[i]);
    }
}

TEST(VulkanEnumTest, BufferMemoryUsageMapping)
{
    constexpr VmaMemoryUsage Expected[] = { VMA_MEMORY_USAGE_GPU_ONLY, VMA_MEMORY_USAGE_CPU_ONLY, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_MEMORY_USAGE_GPU_TO_CPU };

    constexpr uint32_t TestCases = sizeof(Expected) / sizeof(VmaMemoryUsage);
    EXPECT_EQ(TestCases, BufferMemoryUsage::NH3D_BUFFER_MEMORY_USAGE_MAX);

    for (uint32_t i = 0; i < std::max(TestCases, static_cast<uint32_t>(BufferMemoryUsage::NH3D_BUFFER_MEMORY_USAGE_MAX)); ++i) {
        EXPECT_EQ(MapBufferMemoryUsage(static_cast<BufferMemoryUsage>(i)), Expected[i]);
    }
}

}