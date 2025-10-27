#pragma once

#include <scene/ecs/entity.hpp>

namespace NH3D {

class Scene;

// Beware of updates of two nodes in the same subtree by two different threads, race condition
// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct HierarchyComponent {
    Entity parent;

    // TODO: add reparenting functions

    // World space, not unconditionally thread safe
    void translate(Scene& scene, const Entity self, const vec3& translation);

    // World space, not unconditionally thread safe
    void rotate(Scene& scene, const Entity self, const quat& rotation);

    // World space, not unconditionally thread safe
    void scale(Scene& scene, const Entity self, const vec3& scale);
};

}