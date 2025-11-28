#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <misc/utils.hpp>

namespace NH3D {

template <class T> using Uptr = std::unique_ptr<T>;

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using color3 = vec3;
using color4 = vec4;
using vec2i = glm::ivec2;
using vec3i = glm::ivec3;
using vec4i = glm::ivec4;
using vec2u = glm::uvec2;
using vec3u = glm::uvec3;
using vec4u = glm::uvec4;
using quat = glm::quat;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using mat4x3 = glm::mat4x3;

using uint8 = uint8_t;
using byte = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

// Utility type to pass a stack-allocated array pointer and size without needing a vector or both parameters separately
template <typename T>
concept NonVoid = !std::is_void_v<T>;

template <NonVoid T> struct ArrayWrapper {
    const T* const data;
    const uint32 size; // element count

    ArrayWrapper()
        : data { nullptr }
        , size { 0 }
    {
    }

    template <size_t N>
    ArrayWrapper(const std::array<T, N>& arr)
        : data { arr.data() }
        , size { static_cast<uint32>(arr.size()) }
    {
    }

    ArrayWrapper(const std::vector<T>& vec)
        : data { vec.data() }
        , size { static_cast<uint32>(vec.size()) }
    {
    }

    ArrayWrapper(const T& value)
        : data { &value }
        , size { 1 }
    {
    }

    ArrayWrapper(const T* const p, const uint32 size)
        : data { p }
        , size { size }
    {
    }

    template <size_t N>
    ArrayWrapper(const T (&arr)[N])
        : data { arr }
        , size { static_cast<uint32>(N) }
    {
    }

    [[nodiscard]] inline bool isValid() const { return data != nullptr && size > 0; }

    T& operator[](const uint32 index)
    {
        NH3D_ASSERT(index < size, "ArrayWrapper index out of bounds");
        return data[index];
    }

    const T& operator[](const uint32 index) const
    {
        NH3D_ASSERT(index < size, "ArrayWrapper index out of bounds");
        return data[index];
    }
};

struct VertexData {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

}