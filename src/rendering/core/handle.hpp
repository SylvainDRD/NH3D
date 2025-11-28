#pragma once

#include <misc/types.hpp>

namespace NH3D {

template <typename T> struct Handle;

template <typename T> static constexpr Handle<T> InvalidHandle = { NH3D_MAX_T(uint32) };

// using a dummy template parameter makes it a different type for each resource -> some type safety
template <typename T> struct Handle {
    uint32 index; // TODO: default init to InvalidHandle?

    using ResourceType = T;

    constexpr bool operator==(const Handle<T>& other) const noexcept { return index == other.index; }

    constexpr bool operator!=(const Handle<T>& other) const noexcept { return index != other.index; }
};

} // namespace NH3D
