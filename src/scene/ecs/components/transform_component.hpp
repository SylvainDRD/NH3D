#pragma once

#include <misc/types.hpp>
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/entity.hpp>

namespace NH3D {

class Scene;

// Beware of updates of two nodes in the same subtree by two different threads, race condition
// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct TransformComponent {

    // World space, not unconditionally thread safe
    void translate(Scene& scene, const Entity self, const vec3& translation);

    // World space, not unconditionally thread safe
    void rotate(Scene& scene, const Entity self, const quat& rotation);

    // World space, not unconditionally thread safe
    void scale(Scene& scene, const Entity self, const vec3& scale);

private:
    quat _rotation;
    vec3 _position;
    vec3 _scale;

    friend HierarchyComponent;
};

}