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
    vec3 position;
    vec3 normal;
    vec2 uv;
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
