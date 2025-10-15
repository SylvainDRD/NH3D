#pragma once

#include <misc/utils.hpp>

namespace NH3D {

class ISparseSet {
    virtual ~ISparseSet() = default;
};

template <typename T>
class SparseSet : public ISparseSet {
    NH3D_NO_COPY(SparseSet<T>)
public:
private:
    // TODO
};

}