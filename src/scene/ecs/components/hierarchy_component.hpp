#pragma once

#include <scene/ecs/entity.hpp>

namespace NH3D {

class Scene;

// Beware of updates of two nodes in the same subtree by two different threads, race condition
// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct HierarchyComponent {
    static void create(Scene& scene, const Entity entity, const Entity parent);

    [[nodiscard]] Entity parent() const;

    void setParent(const Scene& scene, const Entity parent);

    // World space, not unconditionally thread safe
    void translate(Scene& scene, const Entity self, const vec3& translation);

    // World space, not unconditionally thread safe
    void rotate(Scene& scene, const Entity self, const quat& rotation);

    // World space, not unconditionally thread safe
    void scale(Scene& scene, const Entity self, const vec3& scale);

private:
    Entity _parent;

    template <typename T>
    friend class SparseSet;
};

}