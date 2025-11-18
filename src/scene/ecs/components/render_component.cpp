#include "render_component.hpp"

namespace NH3D {

RenderComponent::RenderComponent(const GPUMesh& meshData, const Material& material)
    : _mesh { meshData }
    , _material { material }
{
}

[[nodiscard]] Handle<Buffer> RenderComponent::getVertexBuffer() const { return _mesh.vertexBuffer; }

[[nodiscard]] Handle<Buffer> RenderComponent::getIndexBuffer() const { return _mesh.indexBuffer; }

[[nodiscard]] const Material& RenderComponent::getMaterial() const { return _material; }

}