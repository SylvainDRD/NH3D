#pragma once

#include <misc/types.hpp>
#include <rendering/core/enums.hpp>

namespace NH3D {

struct Buffer {
    struct CreateInfo {
        uint32 size; // in bytes
        BufferUsageFlags usage;
        BufferMemoryUsage memory;
        const ArrayWrapper<byte> initialData; // optional
    };
};

}