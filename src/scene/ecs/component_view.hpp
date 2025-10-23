#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <tuple>
#include <type_traits>

namespace NH3D {

template <typename... Ts>
class ComponentView {
public:
    ComponentView() = delete;

    ComponentView(std::tuple<SparseSet<std::remove_cvref_t<Ts>>&...> sets);

    class Iterator {
    public:
        Iterator() = delete;

        Iterator(std::tuple<SparseSet<std::remove_cvref_t<Ts>>&...>& sets)
            : _sets { sets }
            , _entities { selectEntityArray() }
        {
        }

        Iterator& operator++()
        {
            ++_id;
            return *this;
        }

        bool operator==(const Iterator& it)
        {
            return _id == it._id;
        }

        bool operator!=(const Iterator& it)
        {
            return !(_id == it._id);
        }

        std::tuple<Entity, Ts...> operator*()
        {
            const Entity e = _entities[_id];

            // TODO: getRaw on main set
            return std::tie(e, std::get<SparseSet<std::remove_cvref_t<Ts>>&>(_sets).get(e)...);
        }

    private:
        template <typename F, int i = 0>
        constexpr void foreachSet(const F& f)
        {
            if constexpr (i < sizeof...(Ts)) {
                f(std::get<i>(_sets));
                foreachSet<F, i + 1>(f);
            }
        }

        const std::vector<Entity>& selectEntityArray()
        {
            const std::vector<Entity>* entities;
            size_t size = NH3D_MAX_T(size_t);

            const auto f = [&size, &entities]<typename T>(const SparseSet<T>& set) {
                if (set.size() < size) {
                    size = set.size();
                    entities = &set.entities();
                }
            };

            foreachSet(f);

            NH3D_ASSERT(entities != nullptr, "Failed to select an entities array");

            return *entities;
        }

    private:
        std::tuple<SparseSet<std::remove_cvref_t<Ts>>&...>& _sets;

        const std::vector<Entity>& _entities;

        uint32 _id = 0;

        friend ComponentView<Ts...>;
    };

    Iterator begin();

    Iterator end();

private:
    std::tuple<SparseSet<std::remove_cvref_t<Ts>>&...> _sets;
};

template <typename... Ts>

ComponentView<Ts...>::ComponentView(std::tuple<SparseSet<std::remove_cvref_t<Ts>>&...> sets)
    : _sets { sets }
{
}

template <typename... Ts>
ComponentView<Ts...>::Iterator ComponentView<Ts...>::begin()
{
    return ComponentView<Ts...>::Iterator { _sets };
}

template <typename... Ts>
ComponentView<Ts...>::Iterator ComponentView<Ts...>::end()
{
    auto it = ComponentView<Ts...>::Iterator { _sets };
    it._id = it._entities.size();
    
    return it;
}

}