#include "vulkan_texture.hpp"
#include <misc/utils.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

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
        .usage = info.generateMipMaps ? (info.usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) : info.usage,
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
            .aspectMask = info.aspect, 
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

    // TODO: handle initial data/texture size mismatch
    if (info.initialData.ptr != nullptr) {
        auto [stagingBuffer, stagingAllocation] = VulkanBuffer::create(rhi,
            {   
                .size = info.initialData.size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                .initialData = { 
                    info.initialData.ptr, 
                    info.initialData.size,
                },
            });

        rhi.executeImmediateCommandBuffer([&](VkCommandBuffer commandBuffer) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const VkBufferImageCopy copyRegion {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = { 
                    .aspectMask = info.aspect, 
                    .mipLevel = 0,
                    .baseArrayLayer = 0, 
                    .layerCount = 1, 
                },
                .imageOffset = { 0, 0, 0 },
                .imageExtent = info.extent,
            };
            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);
        });

        VulkanBuffer::release(rhi, stagingBuffer, stagingAllocation);
    }

    VkSampler sampler = nullptr;
    if (info.createSampler) {
        const VkSamplerCreateInfo samplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        };
        if (vkCreateSampler(rhi.getVkDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            NH3D_ABORT_VK("Failed to create Vulkan sampler");
        }
    }

    if (info.generateMipMaps) {
        if (!info.createSampler || (info.initialData.ptr == nullptr || info.initialData.size == 0)) {
            NH3D_WARN("MipMap generation without sampler or data is weird");
        }
        if (info.extent.depth != 1 || (info.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
            NH3D_ABORT("Mipmapping requires 2D sampled images");
        }

        rhi.executeImmediateCommandBuffer([&](VkCommandBuffer commandBuffer) {
            VkImageLayout previousMipLayout = layout;
            for (uint32 i = 1; i < imageCreateInfo.mipLevels; ++i) {
                const VkImageBlit blit {
                    .srcSubresource = { .aspectMask = info.aspect, .mipLevel = i - 1, .baseArrayLayer = 0, .layerCount = 1, },
                    .srcOffsets = { { 0, 0, 0 },
                        { 
                            std::max(1, static_cast<int>(info.extent.width) >> (i - 1)),
                            std::max(1, static_cast<int>(info.extent.height) >> (i - 1)),
                            static_cast<int>(info.extent.depth), }, },
                    .dstSubresource = { .aspectMask = info.aspect, .mipLevel = i, .baseArrayLayer = 0, .layerCount = 1, },
                    .dstOffsets = { { 0, 0, 0 },
                        { 
                            std::max(1, static_cast<int>(info.extent.width) >> i),
                            std::max(1, static_cast<int>(info.extent.height) >> i),
                            static_cast<int>(info.extent.depth), }, },
                };

                // Prepare previous mip level for reading & next mip level for writing
                VkImageLayout currentMipLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                VulkanTexture::changeLayoutBarrier(commandBuffer, image, previousMipLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1);
                VulkanTexture::changeLayoutBarrier(commandBuffer, image, currentMipLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i, 1);

                vkCmdBlitImage(commandBuffer, image, previousMipLayout, image, currentMipLayout, 1, &blit, VK_FILTER_LINEAR);

                // Transition previous mip level to SHADER_READ_ONLY_OPTIMAL
                VulkanTexture::changeLayoutBarrier(
                    commandBuffer, image, previousMipLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, i - 1, 1);
                previousMipLayout = currentMipLayout;
            }
            VulkanTexture::changeLayoutBarrier(
                commandBuffer, image, previousMipLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, imageCreateInfo.mipLevels - 1, 1);

            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        });
    }

    rhi.executeImmediateCommandBuffer([&](VkCommandBuffer commandBuffer) {
        if (info.usage & VK_IMAGE_USAGE_SAMPLED_BIT && layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (info.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT && layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        } else if (info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT && layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        } else if (info.usage & VK_IMAGE_USAGE_STORAGE_BIT && layout != VK_IMAGE_LAYOUT_GENERAL) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_GENERAL);
        } else if (layout != VK_IMAGE_LAYOUT_GENERAL) {
            VulkanTexture::changeLayoutBarrier(commandBuffer, image, layout, VK_IMAGE_LAYOUT_GENERAL);
        }
    });

    return {
        ImageView {
            image,
            view,
        },
        TextureMetadata {
            .format = info.format,
            .extent = info.extent,
            .sampler = sampler,
            .layout = layout,
            .allocation = allocation,
        },
    };
}

[[nodiscard]] std::pair<ImageView, TextureMetadata> VulkanTexture::wrapSwapchainImage(
    const VulkanRHI& rhi, const VkImage image, const VkFormat format, const VkExtent3D extent, const VkImageAspectFlags aspect)
{
    NH3D_ASSERT(extent.depth == 1, "Swapchain images must be 2D");
    VkImageViewCreateInfo viewCreateInfo { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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
            .sampler = nullptr,
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .allocation = nullptr,
        },
    };
}

void VulkanTexture::release(const IRHI& rhi, ImageView& imageViewData, TextureMetadata& metadata)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (metadata.sampler) {
        vkDestroySampler(vrhi.getVkDevice(), metadata.sampler, nullptr);
        metadata.sampler = nullptr;
    }

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

void VulkanTexture::insertBarrier(VkCommandBuffer commandBuffer, const VkImage image, const VkImageLayout layout)
{
    VkImageLayout updatedLayout = layout;
    changeLayoutBarrier(commandBuffer, image, updatedLayout, layout);
}

void VulkanTexture::changeLayoutBarrier(VkCommandBuffer commandBuffer, const VkImage image, VkImageLayout& layout,
    const VkImageLayout newLayout, const uint32_t baseMipLevel, const uint32_t mipLevels)
{
    VkImageMemoryBarrier2 barrier { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, // TODO: improve
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, // TODO: improve
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, // TODO: improve
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT, // TODO: improve
        .oldLayout = layout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = static_cast<VkImageAspectFlags>(
                (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel = baseMipLevel,
            .levelCount = mipLevels,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        }, };
    layout = newLayout;

    VkDependencyInfo depInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void VulkanTexture::clear(VkCommandBuffer commandBuffer, VkImage image, const color4 color)
{
    VkImageSubresourceRange imageRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS
    };
    vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, reinterpret_cast<const VkClearColorValue*>(&color), 1, &imageRange);
}

void VulkanTexture::blit(
    VkCommandBuffer commandBuffer, const VkImage srcImage, const VkExtent3D srcExtent, VkImage dstImage, const VkExtent3D dstExtent)
{
    VkImageBlit imageBlit {
        .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .srcOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), 1 } },
        .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .dstOffsets = { { 0, 0, 0 }, { static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), 1 } },
    };

    vkCmdBlitImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &imageBlit, VK_FILTER_NEAREST);
}

}