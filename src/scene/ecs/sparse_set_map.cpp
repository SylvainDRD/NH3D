#include "sparse_set_map.hpp"

namespace NH3D {

void SparseSetMap::remove(const Entity entity, ComponentMask mask)
{
    NH3D_ASSERT((mask & SparseSetMap::InvalidEntityMask) == 0, "Invalid entity bit set for entity removal");

    // TODO: fix, brakes the hierarchy

    // TODO: investigate perf vs __builtin_ctz
    for (uint32 id = 0; mask != 0; mask >>= 1, ++id) {
        if (mask & 1) {
            NH3D_ASSERT(_sets[id] != nullptr, "Unexpected null sparse set");
            _sets[id]->remove(entity);
        }
    }
}

void SparseSetMap::setParent(const Entity entity, const Entity parent)
{
    getSet<HierarchyComponent>().setParent(entity, parent);
}

}