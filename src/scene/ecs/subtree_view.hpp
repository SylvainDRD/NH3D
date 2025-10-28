#pragma once

#include "scene/ecs/components/transform_component.hpp"
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/entity.hpp>
#include <vector>

namespace NH3D {

class SubtreeView {
public:
    SubtreeView() = delete;

    SubtreeView(const Entity* const entities, const HierarchyComponent* const hierarchy, const uint32 size);

    class Iterator {
    public:
        Iterator& operator++();

        Entity operator*();

        bool operator==(const Iterator& other);

        bool operator!=(const Iterator& other);

    private:
        Iterator(const SubtreeView& view);

    private:
        const SubtreeView& _view;

        uint32 _id = 0;

        friend SubtreeView;
    };

    Iterator begin() const;

    Iterator end() const;

private:
    const Entity* const _entities;
    const HierarchyComponent* const _hierarchy;
    const uint32 _size;

    // Preallocated buffer used as a stack for subtree iteration
    static std::vector<Entity> g_subtreeParentStack;
};

}