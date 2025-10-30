#pragma once

#include <cstdint>

namespace NH3D {

enum class TextureFormat : uint32_t {
    R8_UINT,
    R8_SINT,
    R16_SINT,
    R16_UINT,
    R16_SFLOAT,
    R32_SINT,
    R32_UINT,
    R32_SFLOAT,
    RG8_UINT,
    RG8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    RG16_UINT,
    RG16_SINT,
    RG16_SFLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG32_UINT,
    RG32_SINT,
    RG32_SFLOAT,
    RGB8_UINT,
    RGB8_SINT,
    RGB8_SRGB,
    RGB8_UNORM,
    RGB8_SNORM,
    RGB16_UINT,
    RGB16_SINT,
    RGB16_SFLOAT,
    RGB16_UNORM,
    RGB16_SNORM,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_SFLOAT,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_SRGB,
    RGBA8_UNORM,
    RGBA8_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_SFLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_SFLOAT,
    NH3D_TEXTURE_FORMAT_MAX
};

// TODO: check if DX12 friendly
using TextureUsageFlags = uint32_t;
enum TextureUsageFlagBits : uint32_t {
    USAGE_COLOR_BIT = 1U << 0,
    USAGE_DEPTH_STENCIL_BIT = 1U << 1,
    USAGE_STORAGE_BIT = 1U << 2,
    USAGE_SAMPLED_BIT = 1U << 3,
    USAGE_INPUT_BIT = 1U << 4,
    NH3D_TEXTURE_USAGE_MAX = 1U << 5
};

// TODO: check if DX12 friendly
using TextureAspectFlags = uint32_t;
enum TextureAspectFlagBits : uint32_t {
    ASPECT_COLOR_BIT = 1U << 0,
    ASPECT_DEPTH_BIT = 1U << 1,
    ASPECT_STENCIL_BIT = 1U << 2,
    NH3D_TEXTURE_ASPECT_MAX = 1U << 3
};

using BufferUsageFlags = uint32_t;
enum BufferUsageFlagBits : uint32_t {
    STORAGE_BUFFER_BIT = 1U << 0,
    SRC_TRANSFER_BIT = 1U << 1,
    DST_TRANSFER_BIT = 1U << 2,
    NH3D_BUFFER_USAGE_MAX = 1U << 3
};

enum class BufferMemoryUsage : uint32_t {
    GPU_ONLY = 0,
    CPU_ONLY,
    CPU_GPU,
    GPU_CPU, // wtf is the difference
    NH3D_BUFFER_MEMORY_USAGE_MAX
};

}