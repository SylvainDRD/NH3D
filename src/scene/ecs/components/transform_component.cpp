#include "transform_component.hpp"
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/scene.hpp>

namespace NH3D {

void TransformComponent::translate(Scene& scene, const Entity self, const vec3& translation)
{
    if (!scene.checkComponents<HierarchyComponent>(self)) {
        _position += translation;
    } else {
        scene.get<HierarchyComponent>(self).translate(scene, self, translation);
    }
}

void TransformComponent::rotate(Scene& scene, const Entity self, const quat& rotation)
{
    if (!scene.checkComponents<HierarchyComponent>(self)) {
        _rotation *= rotation;
    } else {
        scene.get<HierarchyComponent>(self).rotate(scene, self, rotation);
    }
}

void TransformComponent::scale(Scene& scene, const Entity self, const vec3& scale)
{
    if (!scene.checkComponents<HierarchyComponent>(self)) {
        _scale *= scale;
    } else {
        scene.get<HierarchyComponent>(self).scale(scene, self, scale);
    }
}

}