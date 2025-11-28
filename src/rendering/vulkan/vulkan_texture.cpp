#include "vulkan_texture.hpp"
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

uint32 channelCount(const VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
        return 1;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
        return 2;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
        return 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return 4;
    default:
        NH3D_ABORT("Unsupported format for channel count retrieval");
        return 0;
    }
}

[[nodiscard]] std::pair<ImageView, TextureMetadata> VulkanTexture::create(const VulkanRHI& rhi, const CreateInfo& info)
{
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    const VkImageCreateInfo imageCreateInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = info.extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
        .format = info.format,
        .extent = info.extent,
        .mipLevels = info.generateMipMaps ? static_cast<uint32_t>(floor(log2(std::max(info.extent.width, info.extent.height))) + 1) : 1,
        .arrayLayers = 1, // TODO: use that for 3D textures?
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage
        = info.generateMipMaps ? (info.usageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) : info.usageFlags,
        .initialLayout = layout,
    };

    const VmaAllocationCreateInfo allocCreateInfo {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags { VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT },
    };

    VkImage image;
    VmaAllocation allocation;
    if (vmaCreateImage(rhi.getAllocator(), &imageCreateInfo, &allocCreateInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        NH3D_ABORT_VK("VMA image creation failed");
    }

    const VkImageViewCreateInfo viewCreateInfo { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = info.extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format = info.format,
        .subresourceRange = {
            .aspectMask = info.aspectFlags,
            .baseMipLevel = 0,
            .levelCount = imageCreateInfo.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkImageView view;
    if (vkCreateImageView(rhi.getVkDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan image view");
    }

    // TODO: handle initial data/texture size mismatch for other formats? (see below)
    if (info.initialData.data != nullptr
        && info.initialData.size == info.extent.width * info.extent.height * info.extent.depth * channelCount(info.format)) {
        auto [stagingBuffer, stagingAllocation] = VulkanBuffer::create(rhi,
            {
                .size = info.initialData.size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                .initialData = {
                    info.initialData.data,
                    info.initialData.size,
                },
            });
        // TODO: I'm writing bytes but what if the format is not 8-bit based? Might need to adjust that
        rhi.executeImmediateCommandBuffer([&](VkCommandBuffer commandBuffer) {
            VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_UNDEFINED, false, 0, 1);

            const VkBufferImageCopy copyRegion {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = info.aspectFlags,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = { 0, 0, 0 },
                .imageExtent = info.extent,
            };
            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            if (info.generateMipMaps) {
                VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false, 0, 1);

                VkImageLayout previousMipLayout = layout;
                for (uint32 i = 1; i < imageCreateInfo.mipLevels; ++i) {
                    const VkImageBlit blit {
                    .srcSubresource = { .aspectMask = info.aspectFlags, .mipLevel = i - 1, .baseArrayLayer = 0, .layerCount = 1, },
                    .srcOffsets = { { 0, 0, 0 },
                        {
                            std::max(1, static_cast<int>(info.extent.width) >> (i - 1)),
                            std::max(1, static_cast<int>(info.extent.height) >> (i - 1)),
                            static_cast<int>(info.extent.depth), }, },
                    .dstSubresource = { .aspectMask = info.aspectFlags, .mipLevel = i, .baseArrayLayer = 0, .layerCount = 1, },
                    .dstOffsets = { { 0, 0, 0 },
                        {
                            std::max(1, static_cast<int>(info.extent.width) >> i),
                            std::max(1, static_cast<int>(info.extent.height) >> i),
                            static_cast<int>(info.extent.depth), }, },
                };

                    // Prepare previous mip level for reading & next mip level for writing
                    VkImageLayout currentMipLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, previousMipLayout, false, i - 1, 1);
                    VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        currentMipLayout, false, i, 1);

                    vkCmdBlitImage(commandBuffer, image, previousMipLayout, image, currentMipLayout, 1, &blit, VK_FILTER_LINEAR);

                    // Transition previous mip level to SHADER_READ_ONLY_OPTIMAL
                    VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, previousMipLayout, false, i - 1, 1);
                    previousMipLayout = currentMipLayout;
                }
                VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, previousMipLayout, false, imageCreateInfo.mipLevels - 1, 1);
            } else {
                VulkanTexture::insertMemoryBarrier(commandBuffer, image, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false, 0, 1);
            }
        });

        VulkanBuffer::release(rhi, stagingBuffer, stagingAllocation);
    }

    return {
        ImageView {
            image,
            view,
        },
        TextureMetadata {
            .format = info.format,
            .extent = info.extent,
            .allocation = allocation,
        },
    };
}

[[nodiscard]] std::pair<ImageView, TextureMetadata> VulkanTexture::wrapSwapchainImage(
    const VulkanRHI& rhi, const VkImage image, const VkFormat format, const VkExtent3D extent, const VkImageAspectFlags aspect)
{
    NH3D_ASSERT(extent.depth == 1, "Swapchain images must be 2D");
    const VkImageViewCreateInfo viewCreateInfo { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { .aspectMask = aspect, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, }, };

    VkImageView view;
    if (vkCreateImageView(rhi.getVkDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan image view creation failed");
    }

    return {
        ImageView {
            image,
            view,
        },
        TextureMetadata {
            .format = format,
            .extent = extent,
            .allocation = nullptr,
        },
    };
}

void VulkanTexture::release(const IRHI& rhi, ImageView& imageViewData, TextureMetadata& metadata)
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

bool VulkanTexture::valid(const ImageView& imageViewData, const TextureMetadata& /*metadata*/)
{
    // The allocation can be null in case of a swapchain image
    return imageViewData.image != nullptr && imageViewData.view != nullptr;
}

void VulkanTexture::insertMemoryBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkAccessFlags2 srcAccessMask,
    const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 dstAccessMask, const VkPipelineStageFlags2 dstStageMask,
    const VkImageLayout newLayout, const VkImageLayout oldLayout, const bool isDepth, const uint32_t baseMipLevel, const uint32_t mipLevels)
{
    const VkImageMemoryBarrier2 barrier { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = baseMipLevel,
            .levelCount = mipLevels,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        }, };

    const VkDependencyInfo depInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void VulkanTexture::clearColor(VkCommandBuffer commandBuffer, VkImage image, const color4 color, const VkImageLayout layout)
{
    const VkImageSubresourceRange imageRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    vkCmdClearColorImage(commandBuffer, image, layout, reinterpret_cast<const VkClearColorValue*>(&color), 1, &imageRange);
}

void VulkanTexture::clearDepth(VkCommandBuffer commandBuffer, VkImage image, const float depth, const VkImageLayout layout)
{
    const VkImageSubresourceRange imageRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    const VkClearDepthStencilValue clearValue {
        .depth = depth,
    };

    vkCmdClearDepthStencilImage(commandBuffer, image, layout, &clearValue, 1, &imageRange);
}

void VulkanTexture::blit(
    VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent)
{
    const VkImageBlit imageBlit {
        .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), 1 } },
        .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .dstOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), 1 } },
    };

    vkCmdBlitImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &imageBlit, VK_FILTER_NEAREST);
}

}
