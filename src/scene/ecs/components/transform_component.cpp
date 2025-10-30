#include "transform_component.hpp"
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/scene.hpp>

namespace NH3D {

[[nodiscard]] const quat& TransformComponent::rotation() const
{
    return _rotation;
}

[[nodiscard]] const vec3& TransformComponent::position() const
{
    return _position;
}

[[nodiscard]] const vec3& TransformComponent::scale() const
{
    return _scale;
}

void TransformComponent::setPosition(Scene& scene, const Entity self, const vec3& position)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._position = position;
    }
}

void TransformComponent::setRotation(Scene& scene, const Entity self, const quat& rotation)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._rotation = rotation;
    }
}

void TransformComponent::setScale(Scene& scene, const Entity self, const vec3& scale)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._scale = scale;
    }
}

void TransformComponent::translate(Scene& scene, const Entity self, const vec3& translation)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._position += translation;
    }
}

void TransformComponent::rotate(Scene& scene, const Entity self, const quat& rotation)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._rotation = rotation * scene.get<TransformComponent>(entity)._rotation;
    }
}

void TransformComponent::scale(Scene& scene, const Entity self, const vec3& scale)
{
    for (const Entity entity : scene.getSubtree(self)) {
        scene.get<TransformComponent>(entity)._scale *= scale;
    }
}

}