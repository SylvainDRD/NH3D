#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/material.hpp>
#include <rendering/core/mesh.hpp>
#include <rendering/core/shader.hpp>

namespace NH3D {

struct RenderComponent {
public:
    RenderComponent(const Mesh& mesh, const Material& material);

    [[nodiscard]] const Mesh& getMesh() const;

    [[nodiscard]] const Material& getMaterial() const;

private:
    Mesh _mesh;

    Material _material;

    // TODO: static SparseSet ref to set dirty flag?

    // Handle<Shader> _shader;
};

}
