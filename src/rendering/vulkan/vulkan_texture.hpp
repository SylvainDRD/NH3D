#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/rhi_interface.hpp>
#include <rendering/core/texture.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanRHI;

struct _VulkanTexture {
    using ResourceType = Texture;

    struct Hot {
        VkImage _image;
        VkImageView _view;
    };

    struct Cold {
        VkFormat _format;
        VkExtent3D _extent;
        VkImageLayout _layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocation _allocation = nullptr;
    };
};

class VulkanTexture {
    NH3D_NO_COPY(VulkanTexture)
public:
    VulkanTexture() = default;

    VulkanTexture(const VulkanRHI& rhi, VkImage image, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect);

    VulkanTexture(const VulkanRHI& rhi, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmap = true);

    VulkanTexture(VulkanTexture&& other);

    VulkanTexture& operator=(VulkanTexture&& other);

    void release(const IRHI& rhi);

    [[nodiscard]] inline bool isValid() const { return _image != nullptr; }

    [[nodiscard]] inline uint32_t getWidth() const { return _extent.width; }

    [[nodiscard]] inline uint32_t getHeight() const { return _extent.height; }

    [[nodiscard]] inline VkImageView getView() const { return _view; }

    [[nodiscard]] inline VkFormat getFormat() const { return _format; }

    [[nodiscard]] VkRenderingAttachmentInfo getAttachmentInfo() const;

    void insertBarrier(VkCommandBuffer commandBuffer);

    void changeLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout newLayout);

    void clear(VkCommandBuffer commandBuffer, color4 color);

    void blit(VkCommandBuffer commandBuffer, VulkanTexture& dst);

private:
    VkImage _image;
    VkImageView _view;
    VkFormat _format;
    VkExtent3D _extent;
    VkImageLayout _layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocation _allocation = nullptr;
};

}