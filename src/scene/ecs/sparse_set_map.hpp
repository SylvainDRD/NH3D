#pragma once

#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>

namespace NH3D {

// Maps a type to a container
class SparseSetMap {
    NH3D_NO_COPY_MOVE(SparseSetMap)
public:
    SparseSetMap() = default;

    template <typename... Ts>
    inline component_mask mask() const;

    template <typename T>
    inline T& get(entity e);

    template <typename T>
    inline bool has(entity e) const;

    template <typename... Ts>
    inline void add(entity e, Ts&&... components);

    template <typename T>
    inline void remove(entity e);

    inline void remove(entity e, component_mask mask);

private:
    template <typename T>
    inline uint32 getId();

    template <typename T>
    inline SparseSet<T>& get();

private:
    constexpr static uint8 MaxComponent = sizeof(component_mask) * 8;
    mutable Uptr<ISparseSet> _sets[MaxComponent] = {};

    mutable uint32 _nextPoolIndex = 0;
};

template <typename T>
inline uint32 SparseSetMap::getId()
{
    static const uint32 Index = _nextPoolIndex++;

    NH3D_ASSERT(Index <= MaxComponent, "Maximum amount of component types exceeded, kaboom");

    return Index;
}

template <typename T>
inline SparseSet<T>& SparseSetMap::get()
{
    const uint32 index = getId<T>();

    if (_sets[index] == nullptr) {
        _sets[index] = std::make_unique<SparseSet<T>>();
    }

    NH3D_ASSERT(_sets[index] != nullptr, "Unexpected null sparse set");

    return *static_cast<SparseSet<T>*>(_sets[index].get());
};

template <typename... Ts>
inline component_mask SparseSetMap::mask() const
{
    return ((1 << getId<Ts>()) | ...);
}

template <typename T>
inline T& SparseSetMap::get(entity e)
{
    const SparseSet<T>& set = get<T>();

    return set.get(e);
}

template <typename... Ts>
inline void SparseSetMap::add(entity e, Ts&&... components)
{
    for (auto&& component : { components... }) {
        const SparseSet<decltype(component)>& set = get<decltype(component)>();

        set.add(e, std::forward<decltype(component)>(component));
    }
}

template <typename T>
inline void SparseSetMap::remove(entity e)
{
    const SparseSet<T>& set = get<T>();

    set.remove(e);
}

inline void SparseSetMap::remove(entity e, component_mask mask)
{
    for (uint32 id = 0; mask != 0; mask >>= 1, ++id) {
        if (mask & 1) {
            NH3D_ASSERT(_sets[id] != nullptr, "Component")
            _sets[id]->remove(e);
        }
    }
}

}