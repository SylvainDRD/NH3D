#include "subtree_view.hpp"
#include "misc/utils.hpp"
#include "scene/ecs/components/hierarchy_component.hpp"
#include "scene/ecs/entity.hpp"

namespace NH3D {

std::vector<Entity> SubtreeView::g_subtreeParentStack;

SubtreeView::SubtreeView(const Entity* const entities, const HierarchyComponent* const hierarchy, const uint32 size)
    : _entities { entities }
    , _hierarchy { hierarchy }
    , _size { size }
{
    g_subtreeParentStack.reserve(128);
    g_subtreeParentStack.clear();
    g_subtreeParentStack.emplace_back(entities[0]);

    NH3D_ASSERT(entities != nullptr, "Null entities array provided to SubtreeView");
    NH3D_ASSERT(hierarchy != nullptr, "Null hierarchy array provided to SubtreeView");
}

SubtreeView::Iterator& SubtreeView::Iterator::operator++()
{
    ++_id;

    // A bit of a hack but predetermining the actual size beforehand is costly
    if (_id >= _view._size) {
        return *this;
    }

    // Find the parent entity in the previously visited "nodes"
    while(_view._hierarchy[_id].parent != g_subtreeParentStack.back() && !g_subtreeParentStack.empty()) {
        g_subtreeParentStack.pop_back();
    }

    // Didn't find the parent, this is another subtree, we're done
    if(g_subtreeParentStack.empty()) {
        _id = _view._size;
        return *this;
    }

    // We found the parent
    g_subtreeParentStack.emplace_back(_view._entities[_id]);

    return *this;
}

Entity SubtreeView::Iterator::operator*()
{
    return _view._entities[_id];
}

bool SubtreeView::Iterator::operator==(const SubtreeView::Iterator& other)
{
    return _id == other._id;
}

bool SubtreeView::Iterator::operator!=(const SubtreeView::Iterator& other)
{
    return _id != other._id;
}

SubtreeView::Iterator::Iterator(const SubtreeView& view)
    : _view { view }
{
}

SubtreeView::Iterator SubtreeView::begin() const
{
    return SubtreeView::Iterator { *this };
}

SubtreeView::Iterator SubtreeView::end() const
{
    auto it = SubtreeView::Iterator { *this };
    it._id = _size;
    return it;
}

}