#include "vulkan_texture.hpp"
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

VulkanTexture::VulkanTexture(VulkanRHI* rhi, VkImage image, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect)
    : _image { image }
    , _format { format }
    , _extent { extent }
{
    VkImageViewCreateInfo viewCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = _image,
        .viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 }
    };

    if (vkCreateImageView(rhi->getVkDevice(), &viewCreateInfo, nullptr, &_view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan image view creation failed");
    }
}

VulkanTexture::VulkanTexture(VulkanRHI* rhi, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmap)
    : _format { format }
    , _extent { extent }
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
        .initialLayout = _layout
    };

    VmaAllocationCreateInfo allocCreateInfo {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags { VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT }
    };

    if (vmaCreateImage(rhi->getAllocator(), &imageCreateInfo, &allocCreateInfo, &_image, &_allocation, nullptr) != VK_SUCCESS) {
        NH3D_ABORT_VK("VMA image creation failed");
    }

    VkImageViewCreateInfo viewCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = _image,
        .viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 }
    };

    if (vkCreateImageView(rhi->getVkDevice(), &viewCreateInfo, nullptr, &_view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan image view");
    }
}

VulkanTexture::VulkanTexture(VulkanTexture&& other)
{
    _image = other._image;
    other._image = nullptr;
    _view = other._view;
    other._view = nullptr;
    _allocation = other._allocation;
    other._allocation = nullptr;
    _format = other._format;
    _extent = other._extent;
}

VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other)
{
    // Overwritten texture needs to be cleaned up
    NH3D_ASSERT(_view == nullptr, "Vulkan texture view was not cleaned up before move assignment");
    NH3D_ASSERT(_allocation == nullptr, "Vulkan texture allocation was not cleaned up before move assignment");
    NH3D_ASSERT(_image == nullptr, "Vulkan texture image was not cleaned up before move assignment");

    _image = other._image;
    other._image = nullptr;
    _view = other._view;
    other._view = nullptr;
    _allocation = other._allocation;
    other._allocation = nullptr;
    _format = other._format;
    _extent = other._extent;

    return *this;
}

void VulkanTexture::release(const IRHI& rhi)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (_view) {
        vkDestroyImageView(vrhi.getVkDevice(), _view, nullptr);
        _view = nullptr;
    }

    if (_allocation == nullptr) {
        // Either a swapchain image or weird ass bug
        _image = nullptr;
        return;
    }

    if (_image) {
        vmaDestroyImage(vrhi.getAllocator(), _image, _allocation);
        _image = nullptr;
        _allocation = nullptr;
    }
}

VkRenderingAttachmentInfo VulkanTexture::getAttachmentInfo() const
{
    return {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = _view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
}

void VulkanTexture::insertBarrier(VkCommandBuffer commandBuffer)
{
    changeLayoutBarrier(commandBuffer, _layout);
}

void VulkanTexture::changeLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout newLayout) {
    VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = _layout,
        .newLayout = newLayout,
        .image = _image,
        .subresourceRange = {
            .aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        }
    };
    _layout = newLayout;

    VkDependencyInfo depInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void VulkanTexture::clear(VkCommandBuffer commandBuffer, Color color)
{
    VkImageSubresourceRange imageRange = VkImageSubresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS };
    vkCmdClearColorImage(/*rhi->getCommandBuffer()*/ commandBuffer, _image, VK_IMAGE_LAYOUT_GENERAL, reinterpret_cast<VkClearColorValue*>(&color), 1, &imageRange);
}

void VulkanTexture::blit(VkCommandBuffer commandBuffer, VulkanTexture& dst)
{
    VkImageBlit imageBlit {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(getWidth()), static_cast<int32_t>(getHeight()), 1 } },
        .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .dstOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(dst.getWidth()), static_cast<int32_t>(dst.getHeight()), 1 } },
    };

    vkCmdBlitImage(/*rhi->getCommandBuffer()*/ commandBuffer,
        _image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst._image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageBlit,
        VK_FILTER_NEAREST);
}

}