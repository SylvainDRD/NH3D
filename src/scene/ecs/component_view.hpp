#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/sparse_set.hpp>
#include <tuple>
#include <type_traits>

namespace NH3D {

class Scene;

template <typename T, typename... Ts> class ComponentView {
    using TupleType = std::tuple<SparseSet<std::remove_cvref_t<T>>&, SparseSet<std::remove_cvref_t<Ts>>&...>;

public:
    ComponentView() = delete;

    ComponentView(const std::vector<ComponentMask>& entityMasks, TupleType sets, const ComponentMask mask);

    class Iterator {
    public:
        Iterator() = delete;

        Iterator& operator++()
        {
            const auto& entities = std::get<SparseSet<std::remove_cvref_t<T>>&>(_sets).entities();

            do {
                ++_id;
            } while (_id != entities.size() && !ComponentMasks::checkComponents(_entityMasks[entities[_id]], _mask));

            return *this;
        }

        bool operator==(const Iterator& other) { return _id == other._id; }

        bool operator!=(const Iterator& other) { return !(_id == other._id); }

        std::tuple<Entity, T, Ts...> operator*()
        {
            auto& leadSet = std::get<SparseSet<std::remove_cvref_t<T>>&>(_sets);
            const Entity e = leadSet.entities()[_id];

            return std::tie(e, leadSet.getRaw(_id), std::get<SparseSet<std::remove_cvref_t<Ts>>&>(_sets).get(e)...);
        }

    private:
        Iterator(const std::vector<ComponentMask>& entityMasks, TupleType& sets, const ComponentMask mask)
            : _entityMasks { entityMasks }
            , _sets { sets }
            , _mask { mask }
        {
            const auto& entities = std::get<SparseSet<std::remove_cvref_t<T>>&>(_sets).entities();

            while (_id != entities.size() && !ComponentMasks::checkComponents(_entityMasks[entities[_id]], _mask)) {
                ++_id;
            }
        }

    private:
        const std::vector<ComponentMask>& _entityMasks;
        TupleType& _sets;
        const ComponentMask _mask;

        uint32 _id = 0;

        friend ComponentView<T, Ts...>;
    };

    Iterator begin();

    Iterator end();

private:
    const std::vector<ComponentMask>& _entityMasks;
    TupleType _sets;
    const ComponentMask _mask;
};

template <typename T, typename... Ts>
ComponentView<T, Ts...>::ComponentView(const std::vector<ComponentMask>& entityMasks, TupleType sets, const ComponentMask mask)
    : _entityMasks { entityMasks }
    , _sets { sets }
    , _mask { mask }
{
}

template <typename T, typename... Ts> ComponentView<T, Ts...>::Iterator ComponentView<T, Ts...>::begin()
{
    return ComponentView<T, Ts...>::Iterator { _entityMasks, _sets, _mask };
}

template <typename T, typename... Ts> ComponentView<T, Ts...>::Iterator ComponentView<T, Ts...>::end()
{
    auto it = ComponentView<T, Ts...>::Iterator { _entityMasks, _sets, _mask };
    it._id = std::get<SparseSet<std::remove_cvref_t<T>>&>(_sets).size();

    return it;
}

}