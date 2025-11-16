#pragma once

#include <misc/types.hpp>
#include <rendering/core/enums.hpp>

namespace NH3D {

struct Texture {
    struct CreateInfo {
        vec3u size;
        TextureFormat format;
        TextureUsageFlags usage = TextureUsageFlagBits::USAGE_COLOR_BIT;
        TextureAspectFlags aspect = TextureAspectFlagBits::ASPECT_COLOR_BIT;
        ArrayWrapper<byte> initialData;
        bool generateMipMaps : 1 = true;
    };
};

}