#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
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
    MeshComponent(const IRHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32_t>& indices);

private:
    Handle<Buffer> _vertexData;
    Handle<Buffer> _indices;
    Buffer::Address _vertexDataAddress;
};

}
