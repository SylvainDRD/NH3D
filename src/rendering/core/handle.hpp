#pragma once

#include <misc/types.hpp>

namespace NH3D {

// using a dummy template parameter makes it a different type for each resource -> some type safety
template <typename T>
struct Handle {
    uint32 index;
};

}