#pragma once

#include <scene/ecs/entity.hpp>
#include <type_traits>

namespace NH3D {

class HierarchyComponent;
class Scene;

template <typename T>
concept IsHierarchyComponent = requires { std::is_same_v<T, HierarchyComponent>; };

template <typename T>
concept NotHierarchyComponent = requires { (!IsHierarchyComponent<T>); };

// Beware of updates of two nodes in the same subtree by two different threads, race condition
// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct HierarchyComponent {
    [[nodiscard]] Entity parent() const;

    // World space, not unconditionally thread safe
    void translate(Scene& scene, const Entity self, const vec3& translation);

    // World space, not unconditionally thread safe
    void rotate(Scene& scene, const Entity self, const quat& rotation);

    // World space, not unconditionally thread safe
    void scale(Scene& scene, const Entity self, const vec3& scale);

private:
    Entity _parent;

    friend class HierarchySparseSet;
};

}