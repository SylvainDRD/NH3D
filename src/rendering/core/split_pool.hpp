#pragma once

#include "rendering/core/handle.hpp"
#include <misc/utils.hpp>
#include <vector>

namespace NH3D {

template <typename T>
class SplitPool {
    NH3D_NO_COPY(SplitPool)
public:
    SplitPool() = default;

    inline void reserve();

    [[nodiscard]] inline size_t size() const;

    [[nodiscard]] inline typename T::Hot& getHotData(Handle<typename T::ResourceType> handle);

    [[nodiscard]] inline typename T::Cold& getColdData(Handle<typename T::ResourceType> handle);

    [[nodiscard]] inline Handle<typename T::ResourceType> store(typename T::Hot&& hotData, typename T::Cold&& coldData); 
    
    inline void release(Handle<typename T::ResourceType>); 

private:
    std::vector<typename T::Hot> _hot;
    std::vector<typename T::Cold> _cold;
    std::vector<Handle<typename T::ResourceType>> _availableHandles;
};

// TODO

}