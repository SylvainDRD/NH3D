#pragma once

#include <cstdint>
#include <memory>
#include <misc/math.hpp>
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

// Utility type to pass a stack-allocated array pointer and size without needing a vector or both parameters separately
template <typename T>
concept NonVoid = !std::is_void_v<T>;

template <NonVoid T> struct ArrayWrapper {
    const T* ptr;
    const uint32 size; // element count
};

struct AABB {
    vec3 min;
    vec3 max;
};

}