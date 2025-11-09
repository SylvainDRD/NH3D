#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>

namespace NH3D {

struct VertexData {
    vec4 position;
    vec4 normal;
    vec2 uv;
    vec2 __padding; // Padding to make the size a multiple of 16 bytes
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
