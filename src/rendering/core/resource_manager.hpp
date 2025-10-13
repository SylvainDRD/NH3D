#pragma once

#include <cstddef>
#include <misc/utils.hpp>
#include <misc/types.hpp>
#include <rendering/core/rhi_interface.hpp>
#include <sys/stat.h>
#include <utility>

namespace NH3D {

// TODO: rework eventually, trashes the cache
template <Resource T>
class ResourceAllocator {
    NH3D_NO_COPY_MOVE(ResourceAllocator<T>)
public:
    [[nodiscard]] static inline T& getResource(RID rid);

    static inline void reserve(size_t capacity);

    template <class... Args>
    static inline RID allocate(Args... args);

    static inline void release(const IRHI& rhi, RID rid);

    static inline void clear(const IRHI& rhi);

private:
    static std::vector<T> _resources;
    static std::vector<RID> _availableRids;
};

template <Resource T>
std::vector<T> ResourceAllocator<T>::_resources;

template <Resource T>
std::vector<RID> ResourceAllocator<T>::_availableRids;

template <Resource T>
inline void ResourceAllocator<T>::reserve(size_t capacity) {
    _resources.reserve(capacity);
    _availableRids.reserve(capacity);
}

template <Resource T>
[[nodiscard]] inline T& ResourceAllocator<T>::getResource(RID rid)
{
    NH3D_ASSERT(rid != InvalidRID && rid < _resources.size(), "Attempting to fetch a resource with an invalid RID");

    return _resources[rid];
}

template <Resource T>
template <class... Args>
inline RID ResourceAllocator<T>::allocate(Args... args)
{
    RID rid;

    if (!_availableRids.empty()) {
        rid = _availableRids.back();
        _availableRids.pop_back();
        _resources[rid] = T{std::forward<Args>(args)...};
    } else {
        rid = _resources.size();
        _resources.emplace_back(std::forward<Args>(args)...);
    }

    return rid;
}

template <Resource T>
inline void ResourceAllocator<T>::release(const IRHI& rhi, RID rid)
{
    NH3D_ASSERT(rid != InvalidRID && rid < _resources.size(), "Attempting to fetch a resource with an invalid RID");
    NH3D_ASSERT(_resources[rid].isValid(), "Attempting to release an invalid resource");

    if (!_resources[rid].isValid()) {
        NH3D_WARN("Attempting to release an invalid resource");
        return;
    }
    _resources[rid].release(rhi);
    _availableRids.emplace_back(rid);
}

template <Resource T>
inline void ResourceAllocator<T>::clear(const IRHI& rhi)
{
    for (T& resource : _resources) {
        if (resource.isValid()) {
            resource.release(rhi);
        }
    }

    _resources.clear();
    _availableRids.clear();
}

}