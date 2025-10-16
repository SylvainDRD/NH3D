#pragma once

#include <misc/types.hpp>

namespace NH3D {

struct Texture {

    struct CreateInfo {
        vec3i size;
        
        std::vector<byte> initialData;
    };
};

}