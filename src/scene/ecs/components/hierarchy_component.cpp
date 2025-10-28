#include "hierarchy_component.hpp"
#include <scene/ecs/components/transform_component.hpp>
#include <scene/scene.hpp>

namespace NH3D {

[[nodiscard]] void HierarchyComponent::create(Scene& scene, const Entity entity, const Entity parent)
{
    scene.add(entity, HierarchyComponent { parent });
}

[[nodiscard]] Entity HierarchyComponent::parent() const
{
    return _parent;
}

void HierarchyComponent::setParent(const Scene& scene, const Entity parent)
{
    scene.set
}

void HierarchyComponent::translate(Scene& scene, const Entity self, const vec3& translation)
{
    for (const Entity e : scene.getSubtree(self)) {
        scene.get<TransformComponent>(e)._position += translation;
    }
}

void HierarchyComponent::rotate(Scene& scene, const Entity self, const quat& rotation)
{
    for (const Entity e : scene.getSubtree(self)) {
        scene.get<TransformComponent>(e)._rotation *= rotation;
    }
}

void HierarchyComponent::scale(Scene& scene, const Entity self, const vec3& scale)
{
    for (const Entity e : scene.getSubtree(self)) {
        scene.get<TransformComponent>(e)._scale *= scale;
    }
}

}