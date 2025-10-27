#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/component_view.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <scene/ecs/sparse_set_map.hpp>
#include <utility>

namespace NH3D {

    // TODO: scene cloning for in editor play mode
class Scene {
    NH3D_NO_COPY_MOVE(Scene)
public:
    Scene();

    Scene(const std::filesystem::path& filePath);

    template <typename T>
    [[nodiscard]] inline T& get(const Entity e);

    template <typename... Ts>
    inline Entity create(Ts&&... components);

    template <typename... Ts>
    inline void add(const Entity e, Ts&&... components);

    template <typename... Ts>
    [[nodiscard]] inline bool checkComponents(const Entity e) const;

    template <typename... Ts>
    inline void clearComponents(const Entity e);

    inline void remove(const Entity e);

    template <typename T, typename... Ts>
    [[nodiscard]] inline ComponentView<T, Ts...> makeView();

private:
    SparseSetMap _setMap;
    std::vector<ComponentMask> _entityMasks;
    std::vector<uint32> _availableEntities;
};

template <typename T>
[[nodiscard]] inline T& Scene::get(const Entity e)
{
    return _setMap.get<T>(e);
}

template <typename... Ts>
inline Entity Scene::create(Ts&&... components)
{
    Entity e;
    if (!_availableEntities.empty()) {
        e = _availableEntities.back();
        _availableEntities.pop_back();
    } else {
        e = _entityMasks.size();
        _entityMasks.emplace_back(0);
    }

    add(e, std::forward<Ts>(components)...);
    return e;
}

template <typename... Ts>
inline void Scene::add(const Entity e, Ts&&... components)
{
    const ComponentMask mask = _setMap.mask<Ts...>();

    _setMap.add(e, std::forward<Ts>(components)...);

    _entityMasks[e] |= mask;
}

template <typename... Ts>
[[nodiscard]] inline bool Scene::checkComponents(const Entity e) const
{
    NH3D_ASSERT(e < _entityMasks.size(), "Attempting to check components of a non-existing entity");
    return ComponentMaskUtils::checkComponents(_entityMasks[e], _setMap.mask<Ts...>());
}

template <typename... Ts>
inline void Scene::clearComponents(const Entity e)
{
    NH3D_ASSERT(checkComponents<Ts...>(e), "Entity mask is missing components to delete");

    (_setMap.remove<Ts>(e), ...);

    _entityMasks[e] ^= _setMap.mask<Ts...>();
}

inline void Scene::remove(const Entity e)
{
    NH3D_ASSERT(e < _entityMasks.size(), "Attempting to delete a non-existant entity");
    NH3D_ASSERT(_entityMasks[e] != SparseSetMap::InvalidEntityMask, "Attempting to delete an invalid entity");

    _setMap.remove(e, _entityMasks[e]);
    _entityMasks[e] = SparseSetMap::InvalidEntityMask;
    _availableEntities.emplace_back(e);
}

template <typename T, typename... Ts>
[[nodiscard]] inline ComponentView<T, Ts...> Scene::makeView()
{
    return _setMap.makeView<T, Ts...>(_entityMasks);
}

}