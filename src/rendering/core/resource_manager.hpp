#pragma once

#include <cstddef>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi_interface.hpp>
#include <rendering/core/split_pool.hpp>
#include <rendering/core/texture.hpp>
#include <type_traits>

namespace NH3D {

struct VulkanTexture;
struct VulkanBuffer;

class ResourceManager {
    NH3D_NO_COPY_MOVE(ResourceManager)
public:
    ResourceManager();

    template <typename T>
    [[nodiscard]] inline T::Hot& getHotData(Handle<typename T::ResourceType> handle);

    template <typename T>
    [[nodiscard]] inline T::Cold& getColdData(Handle<typename T::ResourceType> handle);

    template <typename T>
    [[nodiscard]] inline Handle<typename T::ResourceType> store(typename T::Hot&& hotData, typename T::Cold&& coldData);

    template <typename T>
    inline void release(const IRHI& rhi, Handle<typename T::ResourceType> handle);

    template <typename T>
    inline void clear(const IRHI& rhi);

private:
    template <typename T>
    constexpr SplitPool<T>& getPool() const;

private:
    // Vulkan
    SplitPool<VulkanTexture> _vulkanTextureData;
    SplitPool<VulkanBuffer> _vulkanBufferData;

    // TODO: DX12 ...

    // Note: tuple based declaration would require feeding every single type at declaration time, which sucks
};

ResourceManager::ResourceManager()
{
    _vulkanTextureData.reserve();
    _vulkanBufferData.reserve();
}

template <typename T>
constexpr SplitPool<T>& ResourceManager::getPool() const
{
    SplitPool<T>* pool;
    if constexpr (std::is_same_v<VulkanTexture, T>) {
        pool = &_vulkanTextureData;
    } else if constexpr (std::is_same_v<VulkanBuffer, T>) {
        pool = &_vulkanBufferData;
    }

    return *pool;
}

template <typename T>
[[nodiscard]] inline typename T::Hot& ResourceManager::getHotData(Handle<typename T::ResourceType> handle)
{
    SplitPool<T> pool = getPool<T>();

    // TODO: customize assertion message using https://github.com/Neargye/nameof
    NH3D_ASSERT(handle.index < pool.size(), "Attempting to fetch a resource with an invalid handle");

    return pool.getHot(handle);
}

template <typename T>
[[nodiscard]] inline typename T::Cold& ResourceManager::getColdData(Handle<typename T::ResourceType> handle)
{
    SplitPool<T> pool = getPool<T>();

    // TODO: customize assertion message using https://github.com/Neargye/nameof
    NH3D_ASSERT(handle.index < pool.size(), "Attempting to fetch a resource with an invalid handle");

    return pool.getCold(handle);
}

template <typename T>
inline Handle<typename T::ResourceType> store(typename T::Hot&& hotData, typename T::Cold&& coldData)
{
    SplitPool<T> pool = getPool<T>();

    return pool.store(std::forward(hotData), std::forward(coldData));
}

template <typename T>
inline void ResourceManager::release(const IRHI& rhi, Handle<typename T::ResourceType> handle)
{
    SplitPool<T> pool = getPool<T>();

    // TODO: customize assertion message using https://github.com/Neargye/nameof
    NH3D_ASSERT(handle.index < pool.size(), "Attempting to fetch a resource with an invalid handle");

    pool.release(handle);
}

inline void ResourceManager::clear(const IRHI& rhi)
{
    _vulkanTextureData.clear(rhi);
    _vulkanBufferData.clear(rhi);
}

}