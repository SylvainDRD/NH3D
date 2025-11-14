#include "render_component.hpp"

namespace NH3D {

RenderComponent::RenderComponent(const GPUMesh& meshData)
    : _mesh { meshData }
{
}

[[nodiscard]] Handle<Buffer> RenderComponent::getVertexBuffer() const { return _mesh.vertexBuffer; }

[[nodiscard]] Handle<Buffer> RenderComponent::getIndexBuffer() const { return _mesh.indexBuffer; }

}