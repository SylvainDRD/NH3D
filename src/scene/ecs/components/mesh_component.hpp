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
public:
    MeshComponent(IRHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32>& indices);

    [[nodiscard]] Handle<Buffer> getVertexBuffer() const;

    [[nodiscard]] Handle<Buffer> getIndexBuffer() const;

private:
    Handle<Buffer> _vertexBuffer;
    Handle<Buffer> _indexBuffer;
};

}
