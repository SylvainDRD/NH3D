#include "vulkan_texture.hpp"
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

std::pair<VulkanTexture::ImageView, VulkanTexture::Metadata> VulkanTexture::create(const VulkanRHI& rhi, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool generateMipMaps)
{
    VkImageCreateInfo imageCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
        .format = format,
        .extent = extent,
        .mipLevels = 1, // TODO
        .arrayLayers = 1, // TODO: use that for 3D textures?
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocCreateInfo {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags { VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT }
    };

    VkImage image;
    VmaAllocation allocation;
    if (vmaCreateImage(rhi.getAllocator(), &imageCreateInfo, &allocCreateInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        NH3D_ABORT_VK("VMA image creation failed");
    }

    VkImageViewCreateInfo viewCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 }
    };

    VkImageView view;
    if (vkCreateImageView(rhi.getVkDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan image view");
    }

    return { ImageView { image, view }, Metadata { format, extent, VK_IMAGE_LAYOUT_UNDEFINED, allocation } };
}

std::pair<VulkanTexture::ImageView, VulkanTexture::Metadata> VulkanTexture::wrapSwapchainImage(const VulkanRHI& rhi, VkImage image, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo viewCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 }
    };

    VkImageView view;
    if (vkCreateImageView(rhi.getVkDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan image view creation failed");
    }

    return { ImageView { image, view }, Metadata { format, extent, VK_IMAGE_LAYOUT_UNDEFINED, nullptr } };
}

void VulkanTexture::release(const IRHI& rhi, ImageView& imageViewData, Metadata& metadata)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (imageViewData.view) {
        vkDestroyImageView(vrhi.getVkDevice(), imageViewData.view, nullptr);
        imageViewData.view = nullptr;
    }

    if (metadata.allocation == nullptr) {
        // Either a swapchain image or weird ass bug
        imageViewData.image = nullptr;
        return;
    }

    if (imageViewData.image) {
        vmaDestroyImage(vrhi.getAllocator(), imageViewData.image, metadata.allocation);
        imageViewData.image = nullptr;
        metadata.allocation = nullptr;
    }
}

void VulkanTexture::insertBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkImageLayout layout)
{
    VkImageLayout updatedLayout = layout;
    changeLayoutBarrier(commandBuffer, image, updatedLayout, layout);
}

void VulkanTexture::changeLayoutBarrier(VkCommandBuffer commandBuffer, const VkImage image, VkImageLayout& layout, const VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, // TODO: improve
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, // TODO: improve
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, // TODO: improve
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT, // TODO: improve
        .oldLayout = layout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = static_cast<VkImageAspectFlags>((newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        }
    };
    layout = newLayout;

    VkDependencyInfo depInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void VulkanTexture::clear(VkCommandBuffer commandBuffer, VkImage image, const color4 color)
{
    VkImageSubresourceRange imageRange = VkImageSubresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS };
    vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, reinterpret_cast<const VkClearColorValue*>(&color), 1, &imageRange);
}

void VulkanTexture::blit(VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent)
{
    VkImageBlit imageBlit {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), 1 } },
        .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .dstOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), 1 } },
    };

    vkCmdBlitImage(commandBuffer,
        srcImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageBlit,
        VK_FILTER_NEAREST);
}

}