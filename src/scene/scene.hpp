#pragma once

#include "scene/ecs/components/hierarchy_component.hpp"
#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/component_view.hpp>
#include <scene/ecs/entity.hpp>
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

    template <typename T>
    [[nodiscard]] inline T& get(const Entity entity);

    [[nodiscard]] inline SubtreeView getSubtree(const Entity entity);

    template <typename... Ts>
    inline Entity create(Ts&&... components);

    template <typename... Ts>
    inline void add(const Entity entity, Ts&&... components)
        requires(!(std::same_as<std::remove_cvref_t<Ts>, HierarchyComponent>) && ...);

    template <typename... Ts>
    [[nodiscard]] inline bool checkComponents(const Entity entity) const;

    template <typename... Ts>
    inline void clearComponents(const Entity entity)
        requires(!(std::same_as<std::remove_cvref_t<Ts>, HierarchyComponent>) && ...);

    template <typename T, typename... Ts>
    [[nodiscard]] inline ComponentView<T, Ts...> makeView();

private:
    SparseSetMap _setMap;
    std::vector<ComponentMask> _entityMasks;
    std::vector<uint32> _availableEntities;
};

template <typename T>
[[nodiscard]] inline T& Scene::get(const Entity entity)
{
    return _setMap.get<T>(entity);
}

[[nodiscard]] inline SubtreeView Scene::getSubtree(const Entity entity)
{
    return _setMap.getSubtree(entity);
}

template <typename... Ts>
inline Entity Scene::create(Ts&&... components)
{
    Entity e;
    if (!_availableEntities.empty()) {
        e = _availableEntities.back();
        _availableEntities.pop_back();
        _entityMasks[e] = _setMap.mask<HierarchyComponent>();
    } else {
        e = _entityMasks.size();
        _entityMasks.emplace_back(_setMap.mask<HierarchyComponent>());
    }

    add(e, std::forward<Ts>(components)...);
    return e;
}

template <typename... Ts>
inline void Scene::add(const Entity entity, Ts&&... components)
    requires(!(std::same_as<std::remove_cvref_t<Ts>, HierarchyComponent>) && ...)
{
    const ComponentMask mask = _setMap.mask<Ts...>();

    _setMap.add(entity, std::forward<Ts>(components)...);

    _entityMasks[entity] |= mask;
}

template <typename... Ts>
[[nodiscard]] inline bool Scene::checkComponents(const Entity entity) const
{
    NH3D_ASSERT(entity < _entityMasks.size(), "Attempting to check components of a non-existing entity");
    return ComponentMaskUtils::checkComponents(_entityMasks[entity], _setMap.mask<Ts...>());
}

template <typename... Ts>
inline void Scene::clearComponents(const Entity entity)
    requires(!(std::same_as<std::remove_cvref_t<Ts>, HierarchyComponent>) && ...)
{
    NH3D_ASSERT(checkComponents<Ts...>(entity), "Entity mask is missing components to delete");

    (_setMap.remove<Ts>(entity), ...);

    _entityMasks[entity] ^= _setMap.mask<Ts...>();
}

template <typename T, typename... Ts>
[[nodiscard]] inline ComponentView<T, Ts...> Scene::makeView()
{
    return _setMap.makeView<T, Ts...>(_entityMasks);
}

}