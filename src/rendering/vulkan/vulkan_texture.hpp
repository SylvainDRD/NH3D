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

struct VulkanTexture {
    using ResourceType = Texture;

    struct ImageView {
        VkImage image;
        VkImageView view;
    };
    using HotType = ImageView;

    struct Metadata {
        VkFormat format;
        VkExtent3D extent;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocation allocation = nullptr;
    };
    using ColdType = Metadata;

    /// "Constructors / Destructors"
    [[nodiscard]] static std::pair<ImageView, Metadata> create(
        const VulkanRHI& rhi, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool generateMipMaps);

    [[nodiscard]] static std::pair<ImageView, Metadata> wrapSwapchainImage(
        const VulkanRHI& rhi, VkImage image, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, ImageView& imageViewData, Metadata& metadata);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const ImageView& imageViewData, const Metadata& metadata)
    {
        return imageViewData.image != nullptr && imageViewData.view != nullptr;
    }

    /// Helper functions
    static void insertBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkImageLayout layout);

    static void changeLayoutBarrier(
        VkCommandBuffer commandBuffer, const VkImage image, VkImageLayout& layout, const VkImageLayout newLayout);

    static void clear(VkCommandBuffer commandBuffer, VkImage image, const color4 color);

    static void blit(VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent);
};

}