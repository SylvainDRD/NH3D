#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <misc/utils.hpp>

namespace NH3D {

template <class T>
using Uptr = std::unique_ptr<T>;

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using color4 = vec4;
using vec2i = glm::ivec2;
using vec3i = glm::ivec3;
using vec4i = glm::ivec4;
using vec2u = glm::uvec2;
using vec3u = glm::uvec3;
using vec4u = glm::uvec4;

using uint8 = uint8_t;
using byte = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

}