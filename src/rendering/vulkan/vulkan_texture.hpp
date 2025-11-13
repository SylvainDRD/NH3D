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
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocation allocation = nullptr;
};

struct VulkanTexture {
    using ResourceType = Texture;

    using HotType = ImageView;

    using ColdType = TextureMetadata;

    /// "Constructors / Destructors"
    [[nodiscard]] static std::pair<ImageView, TextureMetadata> create(const VulkanRHI& rhi, const VkFormat format, const VkExtent3D extent,
        const VkImageUsageFlags usage, const VkImageAspectFlags aspect, const bool generateMipMaps);

    [[nodiscard]] static std::pair<ImageView, TextureMetadata> wrapSwapchainImage(
        const VulkanRHI& rhi, const VkImage image, const VkFormat format, const VkExtent3D extent, const VkImageAspectFlags aspect);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, ImageView& imageViewData, TextureMetadata& metadata);

    // Used generically by the ResourceManager, must be API agnostic
    static bool valid(const ImageView& imageViewData, const TextureMetadata& metadata);

    /// Helper functions
    static void insertBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkImageLayout layout);

    static void changeLayoutBarrier(
        VkCommandBuffer commandBuffer, const VkImage image, VkImageLayout& layout, const VkImageLayout newLayout);

    static void clear(VkCommandBuffer commandBuffer, VkImage image, const color4 color);

    static void blit(
        VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent);
};

}