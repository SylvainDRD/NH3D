#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/split_pool.hpp>
#include <rendering/core/texture.hpp>
#include <type_traits>

namespace NH3D {

struct VulkanTexture;
struct VulkanBuffer;

class ResourceManager {
    NH3D_NO_COPY_MOVE(ResourceManager)
public:
    inline ResourceManager();

    template <typename T>
    [[nodiscard]] inline T::Hot& getHotData(Handle<typename T::ResourceType> handle);

    template <typename T>
    [[nodiscard]] inline T::Cold& getColdData(Handle<typename T::ResourceType> handle);

    template <typename T>
    [[nodiscard]] inline Handle<typename T::ResourceType> store(typename T::Hot&& hotData, typename T::Cold&& coldData);

    template <typename T>
    inline void release(const IRHI& rhi, Handle<typename T::ResourceType> handle);

    inline void clear(const IRHI& rhi);

private:
    template <typename T>
    constexpr SplitPool<T>& getPool();

private:
    // Vulkan
    SplitPool<VulkanTexture> _vulkanTextureData;
    SplitPool<VulkanBuffer> _vulkanBufferData;

    // TODO: DX12 ...

    // Note: tuple based declaration would require feeding every single type at declaration time, which sucks
};

inline ResourceManager::ResourceManager()
    : _vulkanTextureData(20000, 10000)
    , _vulkanBufferData(20000, 10000)
{
}

template <typename T>
constexpr SplitPool<T>& ResourceManager::getPool()
{
    if constexpr (std::is_same_v<VulkanTexture, T>) {
        return _vulkanTextureData;
    } else if constexpr (std::is_same_v<VulkanBuffer, T>) {
        return _vulkanBufferData;
    }
}

template <typename T>
[[nodiscard]] inline typename T::Hot& ResourceManager::getHotData(Handle<typename T::ResourceType> handle)
{
    SplitPool<T>& pool = getPool<T>();

    return pool.getHotData(handle);
}

template <typename T>
[[nodiscard]] inline typename T::Cold& ResourceManager::getColdData(Handle<typename T::ResourceType> handle)
{
    SplitPool<T>& pool = getPool<T>();

    return pool.getColdData(handle);
}

template <typename T>
[[nodiscard]] inline Handle<typename T::ResourceType> ResourceManager::store(typename T::Hot&& hotData, typename T::Cold&& coldData)
{
    SplitPool<T>& pool = getPool<T>();

    return pool.store(std::forward<typename T::Hot>(hotData), std::forward<typename T::Cold>(coldData));
}

template <typename T>
inline void ResourceManager::release(const IRHI& rhi, Handle<typename T::ResourceType> handle)
{
    SplitPool<T>& pool = getPool<T>();

    pool.release(rhi, handle);
}

inline void ResourceManager::clear(const IRHI& rhi)
{
    _vulkanTextureData.clear(rhi);
    _vulkanBufferData.clear(rhi);
}

}