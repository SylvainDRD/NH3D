#pragma once

#include <misc/math.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <vector>

namespace NH3D {

struct AABB {
    vec3 min;
    vec3 max;

    [[nodiscard]] static AABB fromMesh(const std::vector<VertexData>& vertices, const std::vector<uint16>& indices);
};

} // namespace NH3D