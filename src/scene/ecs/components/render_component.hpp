#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/shader.hpp>

namespace NH3D {

struct VertexData {
    vec4 position;
    vec4 normal;
    vec2 uv;
    vec2 __padding; // Padding to make the size a multiple of 16 bytes
};

// Separated because potentially reused w/ other shaders/materials
struct MeshData {
    Handle<Buffer> vertexBuffer;
    Handle<Buffer> indexBuffer;
};

struct RenderComponent {
public:
    RenderComponent(IRHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32>& indices);

    [[nodiscard]] Handle<Buffer> getVertexBuffer() const;

    [[nodiscard]] Handle<Buffer> getIndexBuffer() const;

private:
    MeshData _meshData;

    Handle<BindGroup> _bindGroup; // TODO: material system
    Handle<Shader> _shader; // TODO: material system
};

}
