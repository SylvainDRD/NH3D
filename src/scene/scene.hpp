#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/component_view.hpp>
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/hierarchy_sparse_set.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <scene/ecs/sparse_set_map.hpp>
#include <scene/ecs/subtree_view.hpp>
#include <utility>

namespace NH3D {

// TODO: scene cloning for in editor play mode
class Scene {
    NH3D_NO_COPY_MOVE(Scene)
public:
    Scene();

    Scene(const std::filesystem::path& filePath);

    void remove(const Entity entity);

    void setParent(const Entity entity, const Entity parent);

    template <NotHierarchyComponent T>
    [[nodiscard]] inline T& get(const Entity entity);

    [[nodiscard]] inline SubtreeView getSubtree(const Entity entity);

    template <NotHierarchyComponent... Ts>
    inline Entity create(Ts&&... components);

    template <NotHierarchyComponent... Ts>
    inline void add(const Entity entity, Ts&&... components);

    template <NotHierarchyComponent... Ts>
    [[nodiscard]] inline bool checkComponents(const Entity entity) const;

    template <NotHierarchyComponent... Ts>
    inline void clearComponents(const Entity entity);

    template <NotHierarchyComponent T, NotHierarchyComponent... Ts>
    [[nodiscard]] inline ComponentView<T, Ts...> makeView();

    [[nodiscard]] inline bool isLeaf(const Entity entity) const;

private:
    SparseSetMap _setMap;
    std::vector<ComponentMask> _entityMasks;
    std::vector<uint32> _availableEntities;

    HierarchySparseSet _hierarchy;
};

template <NotHierarchyComponent T>
[[nodiscard]] inline T& Scene::get(const Entity entity)
{
    return _setMap.get<T>(entity);
}

[[nodiscard]] inline SubtreeView Scene::getSubtree(const Entity entity)
{
    // TODO: make this not UB when entity is a root-leaf
    return _hierarchy.getSubtree(entity);
}

template <NotHierarchyComponent... Ts>
inline Entity Scene::create(Ts&&... components)
{
    Entity entity;
    if (!_availableEntities.empty()) {
        entity = _availableEntities.back();
        _availableEntities.pop_back();
        _entityMasks[entity] = 0;
    } else {
        entity = _entityMasks.size();
        _entityMasks.emplace_back(0);
    }

    add(entity, std::forward<Ts>(components)...);
    return entity;
}

template <NotHierarchyComponent... Ts>
inline void Scene::add(const Entity entity, Ts&&... components)
{
    const ComponentMask mask = _setMap.mask<Ts...>();

    _setMap.add(entity, std::forward<Ts>(components)...);

    _entityMasks[entity] |= mask;
}

template <NotHierarchyComponent... Ts>
[[nodiscard]] inline bool Scene::checkComponents(const Entity entity) const
{
    NH3D_ASSERT(entity < _entityMasks.size(), "Attempting to check components of a non-existing entity");
    return ComponentMaskUtils::checkComponents(_entityMasks[entity], _setMap.mask<Ts...>());
}

template <NotHierarchyComponent... Ts>
inline void Scene::clearComponents(const Entity entity)
{
    NH3D_ASSERT(checkComponents<Ts...>(entity), "Entity mask is missing components to delete");

    (_setMap.remove<Ts>(entity), ...);

    _entityMasks[entity] ^= _setMap.mask<Ts...>();
}

template <NotHierarchyComponent T, NotHierarchyComponent... Ts>
[[nodiscard]] inline ComponentView<T, Ts...> Scene::makeView()
{
    return _setMap.makeView<T, Ts...>(_entityMasks);
}

[[nodiscard]] inline bool Scene::isLeaf(const Entity entity) const
{
    return _hierarchy.isLeaf(entity);
}

}