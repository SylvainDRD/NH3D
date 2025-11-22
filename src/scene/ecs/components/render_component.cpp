#include "render_component.hpp"

namespace NH3D {

RenderComponent::RenderComponent(const Mesh& meshData, const Material& material)
    : _mesh { meshData }
    , _material { material }
{
}

[[nodiscard]] const Mesh& RenderComponent::getMesh() const { return _mesh; }

[[nodiscard]] const Material& RenderComponent::getMaterial() const { return _material; }

}