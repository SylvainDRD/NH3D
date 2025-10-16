#pragma once

namespace NH3D {

enum class TextureFormat {
    R8_SINT,
    R8_UINT,
    R16_SINT,
    R16_UINT,
    R16_SFLOAT,
    R32_SINT,
    R32_UINT,
    R32_SFLOAT,
    RG8_SINT,
    RG8_UINT,
    RG16_SINT,
    RG16_UINT,
    RG16_SFLOAT,
    RG32_SINT,
    RG32_UINT,
    RG32_SFLOAT,
    RGB8_SINT,
    RGB8_UINT,
    RGB8_SRGB,
    RGB16_SINT,
    RGB16_UINT,
    RGB16_SFLOAT,
    RGB32_SINT,
    RGB32_UINT,
    RGB32_SFLOAT,
    RGBA8_SINT,
    RGBA8_UINT,
    RGBA8_SRGB,
    RGBA16_SINT,
    RGBA16_UINT,
    RGBA16_SFLOAT,
    RGBA32_SINT,
    RGBA32_UINT,
    RGBA32_SFLOAT,
    MAX
};

enum class BufferUsageFlagBits {
    STORAGE_BUFFER_BIT = 0,
    SRC_TRANSFER_BIT = 1,
    DST_TRANSFER_BIT = 2
};

enum class BufferMemoryUsage {
    GPU_ONLY = 0,
    CPU_ONLY,
    CPU_GPU, 
    GPU_CPU, // wtf is the difference
    MAX
};

}