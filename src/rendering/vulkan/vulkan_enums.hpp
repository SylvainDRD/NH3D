#pragma once

#include "misc/utils.hpp"
#include <misc/types.hpp>
#include <rendering/core/enums.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

inline VkFormat MapTextureFormat(TextureFormat format)
{
    switch (format) {
    case TextureFormat::R8_SINT:
        return VK_FORMAT_R8_SINT;
    case TextureFormat::R8_UINT:
        return VK_FORMAT_R8_UINT;
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
    case TextureFormat::RG8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case TextureFormat::RG8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case TextureFormat::RG16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case TextureFormat::RG16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case TextureFormat::RG16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case TextureFormat::RG32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RG32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case TextureFormat::RG32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case TextureFormat::RGB8_SINT:
        return VK_FORMAT_R8G8B8_SINT;
    case TextureFormat::RGB8_UINT:
        return VK_FORMAT_R8G8B8_UINT;
    case TextureFormat::RGB8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;
    case TextureFormat::RGB16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case TextureFormat::RGB16_UINT:
        return VK_FORMAT_R16G16B16_UINT;
    case TextureFormat::RGB16_SINT:
        return VK_FORMAT_R16G16B16_SINT;
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
    case TextureFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::RGBA16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case TextureFormat::RGBA32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::RGBA32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case TextureFormat::MAX:
        NH3D_ABORT("Invalid texture format");
    }
}

inline VkBufferUsageFlags MapBufferUsageFlagBits(BufferUsageFlagBits flag)
{
    VkBufferUsageFlags vkFlag = 0;

    for (int i = 0; i < sizeof(BufferUsageFlagBits) * 8; ++i) {
        switch (flag) {
        case BufferUsageFlagBits::STORAGE_BUFFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsageFlagBits::SRC_TRANSFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferUsageFlagBits::DST_TRANSFER_BIT:
            vkFlag |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case BufferMemoryUsage::MAX:
        NH3D_ABORT("Invalid BufferMemoryUsage value");
    }
}

}