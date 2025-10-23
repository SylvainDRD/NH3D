#pragma once

#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/component_view.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <type_traits>

namespace NH3D {

// Maps a type to a container
class SparseSetMap {
    NH3D_NO_COPY_MOVE(SparseSetMap)
public:
    SparseSetMap() = default;

    // Most significant bit means invalid entity
    constexpr static uint8 MaxComponent = sizeof(ComponentMask) * 8 - 1;
    static constexpr ComponentMask InvalidEntityMask = 1 << MaxComponent;

    template <typename... Ts>
    [[nodiscard]] inline ComponentMask mask() const;

    template <typename T>
    [[nodiscard]] inline T& get(Entity e);

    template <typename... Ts>
    [[nodiscard]] inline ComponentView<Ts...> makeView();

    template <typename... Ts>
    inline void add(Entity e, Ts&&... components);

    template <typename T>
    inline void remove(Entity e);

    inline void remove(Entity e, ComponentMask mask);

private:
    template <typename T>
    [[nodiscard]] inline uint32 getId() const;

    template <typename T>
    [[nodiscard]] inline SparseSet<T>& getSet();

private:
    mutable Uptr<ISparseSet> _sets[MaxComponent] = {};

    mutable uint32 _nextPoolIndex = 0;
};

template <typename T>
[[nodiscard]] inline uint32 SparseSetMap::getId() const
{
    static const uint32 Index = _nextPoolIndex++;

    NH3D_ASSERT(Index <= MaxComponent, "Maximum amount of component types exceeded, kaboom");

    return Index;
}

template <typename T>
[[nodiscard]] inline SparseSet<T>& SparseSetMap::getSet()
{
    const uint32 index = getId<T>();

    if (_sets[index] == nullptr) {
        _sets[index] = std::make_unique<SparseSet<T>>();
    }

    NH3D_ASSERT(_sets[index] != nullptr, "Unexpected null sparse set");

    return *static_cast<SparseSet<T>*>(_sets[index].get());
};

template <typename... Ts>
[[nodiscard]] inline ComponentMask SparseSetMap::mask() const
{
    return ((1 << getId<Ts>()) | ...);
}

template <typename T>
[[nodiscard]] inline T& SparseSetMap::get(Entity e)
{
    return getSet<T>().get(e);
}

template <typename... Ts>
[[nodiscard]] inline ComponentView<Ts...> SparseSetMap::makeView()
{
    // TODO: decay const/ref qualifiers
    return ComponentView<Ts...> { std::tie(getSet<std::remove_cvref_t<Ts>>()...) };
}

template <typename... Ts>
inline void SparseSetMap::add(Entity e, Ts&&... component)
{
    (getSet<Ts>().add(e, std::forward<Ts>(component)), ...);
}

template <typename T>
inline void SparseSetMap::remove(Entity e)
{
    getSet<T>().remove(e);
}

inline void SparseSetMap::remove(Entity e, ComponentMask mask)
{
    NH3D_ASSERT((mask & SparseSetMap::InvalidEntityMask) == 0, "Invalid entity bit set for entity removal");

    for (uint32 id = 0; mask != 0; mask >>= 1, ++id) {
        if (mask & 1) {
            NH3D_ASSERT(_sets[id] != nullptr, "Unexpected null sparse set");
            _sets[id]->remove(e);
        }
    }
}

}