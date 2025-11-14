#pragma once

#include <cstdint>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/gpu_mesh.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/material.hpp>
#include <rendering/core/shader.hpp>

namespace NH3D {

struct VertexData {
    vec4 position;
    vec4 normal;
    vec2 uv;
    vec2 __padding; // Padding to make the size a multiple of 16 bytes
};

struct RenderComponent {
public:
    RenderComponent(const GPUMesh& mesh);

    [[nodiscard]] Handle<Buffer> getVertexBuffer() const;

    [[nodiscard]] Handle<Buffer> getIndexBuffer() const;

private:
    GPUMesh _mesh;

    Material _material;

    Handle<Shader> _shader;
};

}
