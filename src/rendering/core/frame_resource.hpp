#pragma once

#include <array>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>

namespace NH3D {

// from https://indii.org/blog/is-type-instantiation-of-template/
template <class T, template <class> class U> inline constexpr bool is_instance_of_v = std::false_type {};

template <template <class> class U, class V> inline constexpr bool is_instance_of_v<U<V>, U> = std::true_type {};

template <typename T>
concept ResourceType = std::is_same_v<T, std::remove_cvref_t<T>>;

// Helper to store per-frame resources
template <ResourceType T> struct FrameResource {
    std::array<T, IRHI::MaxFramesInFlight> resources;

    FrameResource()
    {
        if constexpr (is_instance_of_v<T, Handle>) {
            resources.fill(InvalidHandle<typename T::ResourceType>);
        }
    }

    T& operator[](const uint32_t frameInFlightId) { return resources[frameInFlightId]; }

    const T& operator[](const uint32_t frameInFlightId) const { return resources[frameInFlightId]; }
};

}