#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <scene/ecs/sparse_set_map.hpp>
#include <utility>

namespace NH3D {

class EntityManager {
    NH3D_NO_COPY_MOVE(EntityManager)
public:
    static void prealloc();

    template <typename T>
    static inline T& get(entity e);

    template <typename... Ts>
    static inline entity create(Ts&&... components);

    template <typename... Ts>
    static inline void add(entity e, Ts&&... components);

    template <typename T>
    static inline bool has(entity e);

    template <typename T>
    static inline void clearComponent(entity e);

    static inline void remove(entity e);

    // TODO: get component view

private:
    inline EntityManager();

private:
    static SparseSetMap g_setMap;

    static std::vector<component_mask> g_entityMasks;

    static std::vector<uint32> _availableEntities;
};

inline SparseSetMap EntityManager::g_setMap;
inline std::vector<component_mask> EntityManager::g_entityMasks;
inline std::vector<uint32> EntityManager::_availableEntities;

inline void EntityManager::prealloc()
{
    g_entityMasks.reserve(400'000);
    _availableEntities.reserve(2'000);
}

template <typename T>
inline T& EntityManager::get(entity e)
{
    return g_setMap.get<T>(e);
}

template <typename... Ts>
inline entity EntityManager::create(Ts&&... components)
{
    entity e;
    if (!_availableEntities.empty()) {
        e = _availableEntities.back();
        _availableEntities.pop_back();
    } else {
        e = g_entityMasks.size();
        g_entityMasks.emplace_back(0);
    }

    add(e, std::forward<Ts>(components)...);
}

template <typename... Ts>
inline void EntityManager::add(entity e, Ts&&... components)
{
    const component_mask mask = g_setMap.mask<Ts...>();

    g_setMap.add(e, std::forward<Ts>(components)...);

    g_entityMasks[e] |= mask;
}

template <typename T>
inline bool EntityManager::has(entity e)
{
    return g_entityMasks[e] & g_setMap.mask<T>();
}

template <typename T>
inline void EntityManager::clearComponent(entity e)
{
    g_setMap.remove<T>(e);
}

inline void EntityManager::remove(entity e)
{
    g_setMap.remove(e, g_entityMasks[e]);
}

}