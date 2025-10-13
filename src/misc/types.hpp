#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <misc/utils.hpp>

namespace NH3D {

using RIDType = uint32_t;
using RID = RIDType;
static constexpr RID InvalidRID = std::numeric_limits<RID>::max();

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

} 