#pragma once

#include <scene/ecs/entity.hpp>

namespace NH3D {

class ISparseSet {
public:
    virtual ~ISparseSet() = default;

    // This seals the deal as removal being a garbage operation
    // Perhapse if removal becomes performance critical, there is an argument for forward declaring all SparseSet types
    virtual void remove(const Entity entity) = 0;
};

}