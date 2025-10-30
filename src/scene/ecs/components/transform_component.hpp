#pragma once

#include <misc/types.hpp>
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/entity.hpp>

namespace NH3D {

class Scene;

// Beware of updates of two nodes in the same subtree by two different threads, race condition
// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct TransformComponent {
    [[nodiscard]] const quat& rotation() const;

    [[nodiscard]] const vec3& position() const;

    [[nodiscard]] const vec3& scale() const;

    void setPosition(Scene& scene, const Entity self, const vec3& position);

    void setRotation(Scene& scene, const Entity self, const quat& rotation);

    void setScale(Scene& scene, const Entity self, const vec3& scale);

    // World space, not unconditionally thread safe
    void translate(Scene& scene, const Entity self, const vec3& translation);

    // World space, not unconditionally thread safe
    void rotate(Scene& scene, const Entity self, const quat& rotation);

    // World space, not unconditionally thread safe
    void scale(Scene& scene, const Entity self, const vec3& scale);

private:
    quat _rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 _position { 0.0f, 0.0f, 0.0f };
    vec3 _scale { 1.0f, 1.0f, 1.0f };

    friend HierarchyComponent;
};

}