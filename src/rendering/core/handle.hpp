#pragma once

#include <misc/types.hpp>

namespace NH3D {

// using a dummy template parameter makes it a different type for each resource -> some type safety
template <typename T> struct Handle {
    uint32 index;

};

template <typename T> static constexpr Handle<T> InvalidHandle = { NH3D_MAX_T(uint32) };

} // namespace NH3D