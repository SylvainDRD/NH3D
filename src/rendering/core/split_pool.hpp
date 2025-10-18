#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>
#include <utility>
#include <vector>

namespace NH3D {

template <typename T>
class SplitPool {
    NH3D_NO_COPY(SplitPool)
public:
    using Hot = typename T::Hot;
    using Cold = typename T::Cold;
    using HandleType = Handle<typename T::ResourceType>;

    SplitPool(size_t preallocSize, size_t freelistSize);

    [[nodiscard]] inline size_t size() const;

    [[nodiscard]] inline Hot& getHotData(HandleType handle);

    [[nodiscard]] inline Cold& getColdData(HandleType handle);

    [[nodiscard]] inline HandleType store(Hot&& hotData, Cold&& coldData);

    inline void release(const IRHI& rhi, HandleType handle);

    inline void clear(const IRHI& rhi);

private:
    std::vector<Hot> _hot;
    std::vector<Cold> _cold;
    std::vector<HandleType> _availableHandles;
};

template <typename T>
[[nodiscard]] inline SplitPool<T>::SplitPool(size_t preallocSize, size_t freelistSize)
{
    _hot.reserve(preallocSize);
    _cold.reserve(preallocSize);
    _availableHandles.reserve(freelistSize);
}

template <typename T>
[[nodiscard]] inline size_t SplitPool<T>::size() const
{
    NH3D_ASSERT(_hot.size() == _cold.size(), "Hot and cold data size mismatch");
    return _hot.size() - _availableHandles.size();
}

template <typename T>
[[nodiscard]] inline SplitPool<T>::Hot& SplitPool<T>::getHotData(HandleType handle)
{
    NH3D_ASSERT(handle.index < _hot.size(), "Invalid handle index");
    return _hot[handle.index];
}

template <typename T>
[[nodiscard]] inline SplitPool<T>::Cold& SplitPool<T>::getColdData(HandleType handle)
{
    NH3D_ASSERT(handle.index < _cold.size(), "Invalid handle index");
    return _cold[handle.index];
}

template <typename T>
[[nodiscard]] inline SplitPool<T>::HandleType SplitPool<T>::store(Hot&& hotData, Cold&& coldData)
{
    HandleType handle;
    if (!_availableHandles.empty()) {
        handle = _availableHandles.back();
        _availableHandles.pop_back();

        _hot[handle.index] = std::forward<Hot>(hotData);
        _cold[handle.index] = std::forward<Cold>(coldData);
    } else {
        handle = { static_cast<uint32>(_hot.size()) };
        _hot.emplace_back(std::forward<Hot>(hotData));
        _cold.emplace_back(std::forward<Cold>(coldData));
    }


    return handle;
}

template <typename T>
inline void SplitPool<T>::release(const IRHI& rhi, HandleType handle)
{
    NH3D_ASSERT(handle.index < _cold.size(), "Invalid handle index");

    Hot& hot = getHotData(handle);
    Cold& cold = getColdData(handle);

    NH3D_ASSERT(T::valid(hot, cold), "Trying to release an invalid handle");
    T::release(rhi, getHotData(handle), getColdData(handle));
    NH3D_ASSERT(!T::valid(hot, cold), "Released handle should not be valid: resource is not cleaned up properly");

    _availableHandles.emplace_back(handle.index);
}

template <typename T>
inline void SplitPool<T>::clear(const IRHI& rhi)
{
    for (uint32 i = 0; i < _hot.size(); ++i) {
        const HandleType handle { i };
        Hot& hot = getHotData(handle);
        Cold& cold = getColdData(handle);

        if (T::valid(hot, cold)) {
            T::release(rhi, hot, cold);
        }
    }

    _hot.clear();
    _cold.clear();
    _availableHandles.clear();
}

}