#pragma once

#include <misc/utils.hpp>
#include <rendering/core/enums.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

inline VkFormat MapTextureFormat(TextureFormat format)
{
    switch (format) {
    case TextureFormat::R8_UINT:
        return VK_FORMAT_R8_UINT;
    case TextureFormat::R8_SINT:
        return VK_FORMAT_R8_SINT;
    case TextureFormat::R16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case TextureFormat::R16_UINT:
        return VK_FORMAT_R16_UINT;
    case TextureFormat::R16_SINT:
        return VK_FORMAT_R16_SINT;
    case TextureFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::R32_UINT:
        return VK_FORMAT_R32_UINT;
    case TextureFormat::R32_SINT:
        return VK_FORMAT_R32_SINT;
    case TextureFormat::RG8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case TextureFormat::RG8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case TextureFormat::RG8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case TextureFormat::RG8_SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case TextureFormat::RG16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case TextureFormat::RG16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case TextureFormat::RG16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case TextureFormat::RG16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case TextureFormat::RG16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case TextureFormat::RG32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case TextureFormat::RG32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case TextureFormat::RG32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGB8_UINT:
        return VK_FORMAT_R8G8B8_UINT;
    case TextureFormat::RGB8_SINT:
        return VK_FORMAT_R8G8B8_SINT;
    case TextureFormat::RGB8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;
    case TextureFormat::RGB8_UNORM:
        return VK_FORMAT_R8G8B8_UNORM;
    case TextureFormat::RGB8_SNORM:
        return VK_FORMAT_R8G8B8_SNORM;
    case TextureFormat::RGB16_UINT:
        return VK_FORMAT_R16G16B16_UINT;
    case TextureFormat::RGB16_SINT:
        return VK_FORMAT_R16G16B16_SINT;
    case TextureFormat::RGB16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case TextureFormat::RGB16_UNORM:
        return VK_FORMAT_R16G16B16_UNORM;
    case TextureFormat::RGB16_SNORM:
        return VK_FORMAT_R16G16B16_SNORM;
    case TextureFormat::RGB32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case TextureFormat::RGB32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case TextureFormat::RGB32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case TextureFormat::RGBA8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case TextureFormat::RGBA8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case TextureFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureFormat::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case TextureFormat::RGBA16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case TextureFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::RGBA16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case TextureFormat::RGBA16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case TextureFormat::RGBA32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::RGBA32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case TextureFormat::NH3D_TEXTURE_FORMAT_MAX:
        NH3D_ABORT("Invalid texture format");
    }
}

inline VkImageUsageFlags MapTextureUsageFlags(TextureUsageFlags flag)
{
    VkImageUsageFlags vkFlag = 0;

    for (uint32_t i = 1; i < static_cast<uint32_t>(TextureUsageFlagBits::NH3D_TEXTURE_USAGE_MAX); i <<= 1) {
        switch (static_cast<TextureUsageFlagBits>(static_cast<uint32_t>(flag) & i)) {
        case TextureUsageFlagBits::USAGE_COLOR_BIT:
            vkFlag |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        case TextureUsageFlagBits::USAGE_DEPTH_STENCIL_BIT:
            vkFlag |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        case TextureUsageFlagBits::USAGE_STORAGE_BIT:
            vkFlag |= VK_IMAGE_USAGE_STORAGE_BIT;
            break;
        case TextureUsageFlagBits::USAGE_SAMPLED_BIT:
            vkFlag |= VK_IMAGE_USAGE_SAMPLED_BIT;
            break;
        case TextureUsageFlagBits::USAGE_INPUT_BIT:
            vkFlag |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            break;
        case TextureUsageFlagBits::NH3D_TEXTURE_USAGE_MAX:
            NH3D_ABORT("Invalid TextureUsageFlagBits set");
        }
    }

    return vkFlag;
}

inline VkImageAspectFlags MapTextureAspectFlags(TextureAspectFlags flag)
{
    VkImageAspectFlags vkFlag = 0;

    for (uint32_t i = 1; i < static_cast<uint32_t>(TextureAspectFlagBits::NH3D_TEXTURE_ASPECT_MAX); i <<= 1) {
        switch (static_cast<TextureAspectFlagBits>(static_cast<uint32_t>(flag) & i)) {
        case TextureAspectFlagBits::ASPECT_COLOR_BIT:
            vkFlag |= VK_IMAGE_ASPECT_COLOR_BIT;
            break;
        case TextureAspectFlagBits::ASPECT_DEPTH_BIT:
            vkFlag |= VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        case TextureAspectFlagBits::ASPECT_STENCIL_BIT:
            vkFlag |= VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case TextureAspectFlagBits::NH3D_TEXTURE_ASPECT_MAX:
            NH3D_ABORT("Invalid TextureAspectFlagBits set");
        }
    }

    return vkFlag;
}

inline VkBufferUsageFlags MapBufferUsageFlags(BufferUsageFlags flag)
{
    VkBufferUsageFlags vkFlag = 0;

    for (uint32_t i = 1; i < static_cast<uint32_t>(BufferUsageFlagBits::NH3D_BUFFER_USAGE_MAX); i <<= 1) {
        switch (static_cast<BufferUsageFlagBits>(static_cast<uint32_t>(flag) & i)) {
        case BufferUsageFlagBits::STORAGE_BUFFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case BufferUsageFlagBits::SRC_TRANSFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case BufferUsageFlagBits::DST_TRANSFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case BufferUsageFlagBits::NH3D_BUFFER_USAGE_MAX:
            NH3D_ABORT("Invalid BufferUsageFlagBits set");
        }
    }

    return vkFlag;
}

inline VmaMemoryUsage MapBufferMemoryUsage(BufferMemoryUsage flag)
{
    switch (flag) {
    case BufferMemoryUsage::GPU_ONLY:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case BufferMemoryUsage::CPU_ONLY:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case BufferMemoryUsage::CPU_GPU:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferMemoryUsage::GPU_CPU:
        return VMA_MEMORY_USAGE_GPU_TO_CPU;
    case BufferMemoryUsage::NH3D_BUFFER_MEMORY_USAGE_MAX:
        NH3D_ABORT("Invalid BufferMemoryUsage value");
    }
}

}