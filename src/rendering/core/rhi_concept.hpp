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
concept UnmanagedResource = std::movable<T> && Releasable<T>;

template <typename E>
concept IsValidBufferUsageFlagsEnum = std::is_enum_v<E> && requires {
    { E::STORAGE_BUFFER_BIT } -> std::same_as<E>;
    { E::SRC_TRANSFER_BIT } -> std::same_as<E>;
    { E::DST_TRANSFER_BIT } -> std::same_as<E>;
};

template <typename RHI>
concept DefinesBufferUsageFlags = requires { typename RHI::BufferUsageFlagsType; } && IsValidBufferUsageFlagsEnum<typename RHI::BuferUsageFlagsType>;

template <typename E>
concept IsValidMemoryUsageEnum = std::is_enum_v<E> && requires {
    { E::GPU_ONLY } -> std::same_as<E>;
    { E::CPU_ONLY } -> std::same_as<E>;
};

template <typename RHI>
concept DefinesMemoryUsage = requires { typename RHI::MemoryUsageType; } && IsValidMemoryUsageEnum<typename RHI::MemoryUsageeType>;

template <typename T>
concept RHI = std::derived_from<T, IRHI> && DefinesBufferUsageFlags<T> && DefinesMemoryUsage<T>;

}