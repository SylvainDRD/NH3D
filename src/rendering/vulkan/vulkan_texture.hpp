#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/texture.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct ImageView {
    VkImage image;
    VkImageView view;
};

struct TextureMetadata {
    VkFormat format;
    VkExtent3D extent;
    VmaAllocation allocation = nullptr;
};

struct VulkanTexture {
    using ResourceType = Texture;

    using HotType = ImageView;

    using ColdType = TextureMetadata;

    struct CreateInfo {
        const VkImage image; // If not VK_NULL_HANDLE, wraps this image instead of creating a new one
        const VkFormat format;
        const VkExtent3D extent;
        const VkImageUsageFlags usageFlags;
        const VkImageAspectFlags aspectFlags;
        const ArrayWrapper<byte> initialData;
        const bool generateMipMaps : 1;
    };
    using CreateInfo = CreateInfo;

    /// "Constructors / Destructors": used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static std::pair<ImageView, TextureMetadata> create(IRHI& rhi, const CreateInfo& info);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, ImageView& imageViewData, TextureMetadata& metadata);

    // Used generically by the ResourceManager, must be API agnostic
    static bool valid(const ImageView& imageViewData, const TextureMetadata& metadata);

    /// Helper functions
    static void insertMemoryBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkAccessFlags2 srcAccessMask,
        const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 dstAccessMask, const VkPipelineStageFlags2 dstStageMask,
        const VkImageLayout newLayout, const VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, const bool isDepth = false,
        const uint32_t baseMipLevel = 0, const uint32_t mipLevels = VK_REMAINING_MIP_LEVELS);

    static void blit(
        VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent);

private:
    [[nodiscard]] static std::pair<ImageView, TextureMetadata> wrapSwapchainImage(
        const VulkanRHI& rhi, const VkImage image, const VkFormat format, const VkExtent3D extent, const VkImageAspectFlags aspect);
};

}
