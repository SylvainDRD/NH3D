#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/rhi.hpp>

namespace NH3D {

struct VertexData {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct MeshComponent {
    NH3D_NO_COPY(MeshComponent)
public:
    MeshComponent(const std::vector<VertexData>& vertexData, const std::vector<uint32_t>& indices);

private:
    RID _vertexData;
    RID _indices;
    // RHI::BufferAddress _vertexDataGPUAddress;
};

}
