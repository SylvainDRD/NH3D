#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/component_view.hpp>
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
    [[nodiscard]] static inline T& get(Entity e);

    template <typename... Ts>
    [[nodiscard]] static inline Entity create(Ts&&... components);

    template <typename... Ts>
    static inline void add(Entity e, Ts&&... components);

    template <typename... Ts>
    [[nodiscard]] static inline bool has(Entity e);

    template <typename... Ts>
    static inline void clearComponent(Entity e);

    static inline void remove(Entity e);

    template <typename T, typename... Ts>
    [[nodiscard]] static inline ComponentView<T, Ts...> get();

private:
    inline EntityManager();

private:
    static SparseSetMap g_setMap;

    static std::vector<ComponentMask> g_entityMasks;

    static std::vector<uint32> g_availableEntities;
};

inline SparseSetMap EntityManager::g_setMap;
inline std::vector<ComponentMask> EntityManager::g_entityMasks;
inline std::vector<uint32> EntityManager::g_availableEntities;

inline void EntityManager::prealloc()
{
    g_entityMasks.reserve(400'000);
    g_availableEntities.reserve(2'000);
}

template <typename T>
[[nodiscard]] inline T& EntityManager::get(Entity e)
{
    return g_setMap.get<T>(e);
}

template <typename... Ts>
[[nodiscard]] inline Entity EntityManager::create(Ts&&... components)
{
    Entity e;
    if (!g_availableEntities.empty()) {
        e = g_availableEntities.back();
        g_availableEntities.pop_back();
    } else {
        e = g_entityMasks.size();
        g_entityMasks.emplace_back(0);
    }

    add(e, std::forward<Ts>(components)...);
    return e;
}

template <typename... Ts>
inline void EntityManager::add(Entity e, Ts&&... components)
{
    const ComponentMask mask = g_setMap.mask<Ts...>();

    g_setMap.add(e, std::forward<Ts>(components)...);

    g_entityMasks[e] |= mask;
}

template <typename... Ts>
[[nodiscard]] inline bool EntityManager::has(Entity e)
{
    NH3D_ASSERT(e < g_entityMasks.size(), "Trying to check ComponentMask for a non-existing entity");
    return (g_entityMasks[e] & g_setMap.mask<Ts...>()) == g_setMap.mask<Ts...>();
}

template <typename... Ts>
inline void EntityManager::clearComponent(Entity e)
{
    NH3D_ASSERT(has<Ts...>(e), "Entity mask is missing components to delete");

    (g_setMap.remove<Ts>(e), ...);

    g_entityMasks[e] ^= g_setMap.mask<Ts...>();
}

inline void EntityManager::remove(Entity e)
{
    NH3D_ASSERT(e < g_entityMasks.size(), "Attempting to delete a non-existant entity");
    NH3D_ASSERT(g_entityMasks[e] != SparseSetMap::InvalidEntityMask, "Attempting to delete an invalid entity");

    g_setMap.remove(e, g_entityMasks[e]);
    g_entityMasks[e] = SparseSetMap::InvalidEntityMask;
    g_availableEntities.emplace_back(e);
}

template <typename T, typename... Ts>
[[nodiscard]] inline ComponentView<T, Ts...> EntityManager::get()
{
    return g_setMap.getSet<T, Ts...>();
}

}