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
    using Hot = ImageView;

    struct Meta {
        VkFormat format;
        VkExtent3D extent;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocation allocation = nullptr;
    };
    using Cold = Meta;

    // Typically used for swapchain images
    [[nodiscard]] static std::pair<ImageView, Meta> create(const VulkanRHI& rhi, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect);

    [[nodiscard]] static std::pair<ImageView, Meta> create(const VulkanRHI& rhi, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmap = true);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation 
    static void release(const IRHI& rhi, ImageView& imageViewData, Meta& metadata);

    // Used generically by the ResourceManager, must be API agnostic
    [[nodiscard]] static inline bool valid(const ImageView& imageViewData, const Meta& metadata) { return imageViewData._image != nullptr; }

    /// Helper functions
    static void insertBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout layout);

    static void changeLayoutBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout &layout, VkImageLayout newLayout);

    static void clear(VkCommandBuffer commandBuffer, VkImage image,  color4 color);

    static void blit(VkCommandBuffer commandBuffer, VkImage srcImage, VkExtent3D srcExtent, VkImage dstImage, VkExtent3D dstExtent);
};

}