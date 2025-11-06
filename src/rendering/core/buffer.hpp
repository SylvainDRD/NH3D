#pragma once

#include <misc/types.hpp>
#include <rendering/core/enums.hpp>

namespace NH3D {

struct Buffer {
    using Address = uint64;

    struct CreateInfo {
        size_t size; // in bytes
        BufferUsageFlags usage;
        BufferMemoryUsage memory;
    };
};

}