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

// TODO: force RigidBodyComponents only on root HierarchyComponent, disallow otherwise
struct HierarchyComponent {
    [[nodiscard]] Entity parent() const { return _parent; }

private:
    Entity _parent = InvalidEntity;

    friend class HierarchySparseSet;
};

}