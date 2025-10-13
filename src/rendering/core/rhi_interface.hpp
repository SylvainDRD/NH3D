#pragma once

#include <concepts>
#include <misc/utils.hpp>
#include <type_traits>

namespace NH3D {

class RenderGraph;

class IRHI {
    NH3D_NO_COPY_MOVE(IRHI)
public:
    IRHI() = default;

    virtual ~IRHI() = default;

    virtual void render(const RenderGraph& graph) const = 0;
};

template <typename T>
concept Releasable = requires(T resource, const IRHI& rhi) { { resource.release(rhi) } -> std::same_as<void>; };

template <typename T>
concept Resource = std::movable<T> && Releasable<T> && 
requires(T resource) { 
    { resource.isValid() } -> std::same_as<bool>; 
};

template <typename T>
concept RHI = std::derived_from<T, IRHI>;

}