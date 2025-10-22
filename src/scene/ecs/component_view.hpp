#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <tuple>

namespace NH3D {

template <typename... Ts>
class ComponentView {
    NH3D_NO_COPY_MOVE(ComponentView)
public:
    ComponentView() = delete;

    ComponentView(SparseSet<Ts>&... sets);

    class Iterator {
    public:
        Iterator() = delete;

        Iterator(const std::tuple<SparseSet<Ts>&...>& sets)
            : _sets { sets }
            , _entities { selectEntityArray(sets) }
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

        const std::tuple<Ts...>& operator*()
        {
            const Entity e = _entities[_id];

            return std::make_tuple(std::get<Ts>(_sets).get(e)...);
        }

    private:
        const std::vector<Entity>& selectEntityArray(const std::tuple<SparseSet<Ts>&...>& sets)
        {
            const std::vector<Entity>* entities;

            size_t size = NH3D_MAX_T(size_t);
            for (const auto& set : sets) {
                if (set.size() < size) {
                    size = set.size();
                    entities = &set.entities();
                }
            }
            return *entities;
        }

    private:
        const std::tuple<SparseSet<Ts>&...>& _sets;

        const std::vector<Entity>& _entities;

        uint32 _id = 0;
    };

    Iterator begin();

    Iterator end();

private:
    std::tuple<SparseSet<Ts>...> _sets;
};

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
    NH3D_DEBUGLOG("ComponentView::end called");
    return it;
}

}