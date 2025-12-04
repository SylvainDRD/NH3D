#include "aabb.hpp"

namespace NH3D {

[[nodiscard]] AABB AABB::fromMesh(const std::vector<VertexData>& vertices, const std::vector<uint16>& indices)
{
    if (vertices.empty()) {
        return AABB { .min = vec3 { 0.0f }, .max = vec3 { 0.0f } };
    }

    vec3 pMin = vertices[0].position;
    vec3 pMax = vertices[0].position;

    for (uint32 i = 0; i < vertices.size(); ++i) {
        const vec3& pos = vertices[i].position;
        pMin.x = NH3D::min(pMin.x, pos.x);
        pMin.y = NH3D::min(pMin.y, pos.y);
        pMin.z = NH3D::min(pMin.z, pos.z);

        pMax.x = NH3D::max(pMax.x, pos.x);
        pMax.y = NH3D::max(pMax.y, pos.y);
        pMax.z = NH3D::max(pMax.z, pos.z);
    }

    return AABB { .min = pMin, .max = pMax };
}

} // namespace NH3D