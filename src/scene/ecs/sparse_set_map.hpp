#pragma once

#include <vector>
#include <scene/ecs/sparse_set.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>

namespace NH3D {

// Maps a type to a container 
class SparseSetMap {
    NH3D_NO_COPY_MOVE(SparseSetMap)
public:
    template <typename T>
    inline SparseSet<T>& getSet() const;

private:
    mutable std::vector<ISparseSet> _sets;

    mutable uint32 _nextPoolIndex = 0;
};

template <typename T>
inline SparseSet<T>& SparseSetMap::getSet() const
{
    static const uint32 Index = _nextPoolIndex++;

    if (Index < _sets.size()) {
        return static_cast<SparseSet<T>>(_sets[Index]);
    }

    NH3D_ASSERT(Index == _sets.size(), "Unexpected set index in sparse set map");

    _sets.emplace_back();
    return static_cast<SparseSet<T>>(_sets[Index]);
};

}