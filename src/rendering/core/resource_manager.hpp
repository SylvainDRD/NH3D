#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>
#include <utility>
#include <vector>

namespace NH3D {

template <typename T> class ResourceManager {
    NH3D_NO_COPY(ResourceManager)
public:
    using HotType = typename T::HotType;
    using ColdType = typename T::ColdType;
    using HandleType = Handle<typename T::ResourceType>;

    ResourceManager() = delete;

    ResourceManager(const size_t preallocSize, const size_t freelistSize);

    [[nodiscard]] inline size_t size() const;

    template <typename U> [[nodiscard]] inline U& get(HandleType handle);

    [[nodiscard]] inline HandleType store(HotType&& hotData, ColdType&& coldData);

    inline void release(const IRHI& rhi, HandleType handle);

    inline void clear(const IRHI& rhi);

private:
    std::vector<HotType> _hot;
    std::vector<ColdType> _cold;
    std::vector<HandleType> _availableHandles;
};

template <typename T> [[nodiscard]] inline ResourceManager<T>::ResourceManager(const size_t preallocSize, const size_t freelistSize)
{
    _hot.reserve(preallocSize);
    _cold.reserve(preallocSize);
    _availableHandles.reserve(freelistSize);
}

template <typename T> template <typename U> [[nodiscard]] inline U& ResourceManager<T>::get(HandleType handle)
{
    // Cannot reasonnably check that the resource is valid without trashing the cache with cold data, which would destroy the performance
    // This will be checked in a test
    NH3D_ASSERT(handle.index < _hot.size(), "Invalid handle index");

    if constexpr (std::is_same_v<U, HotType>) {
        return _hot[handle.index];
    } else if constexpr (std::is_same_v<U, ColdType>) {
        return _cold[handle.index];
    }
}

template <typename T> [[nodiscard]] inline ResourceManager<T>::HandleType ResourceManager<T>::store(HotType&& hotData, ColdType&& coldData)
{
    HandleType handle;
    if (!_availableHandles.empty()) {
        handle = _availableHandles.back();
        _availableHandles.pop_back();

        _hot[handle.index] = std::forward<HotType>(hotData);
        _cold[handle.index] = std::forward<ColdType>(coldData);
    } else {
        handle = { static_cast<uint32>(_hot.size()) };
        _hot.emplace_back(std::forward<HotType>(hotData));
        _cold.emplace_back(std::forward<ColdType>(coldData));
    }

    return handle;
}

template <typename T> inline void ResourceManager<T>::release(const IRHI& rhi, HandleType handle)
{
    NH3D_ASSERT(handle.index < _cold.size(), "Invalid handle index");

    HotType& hot = get<HotType>(handle);
    ColdType& cold = get<ColdType>(handle);

    NH3D_ASSERT(T::valid(hot, cold), "Trying to release an invalid handle");
    T::release(rhi, hot, cold);
    NH3D_ASSERT(!T::valid(hot, cold), "Released handle should not be valid: resource is not cleaned up properly");

    _availableHandles.emplace_back(handle.index);
}

template <typename T> inline void ResourceManager<T>::clear(const IRHI& rhi)
{
    for (uint32 i = 0; i < _hot.size(); ++i) {
        const HandleType handle { i };
        HotType& hot = get<HotType>(handle);
        ColdType& cold = get<ColdType>(handle);

        if (T::valid(hot, cold)) {
            T::release(rhi, hot, cold);
        }
    }

    _hot.clear();
    _cold.clear();
    _availableHandles.clear();
}

}