#pragma once

#include <misc/types.hpp>
#include <rendering/core/enums.hpp>

namespace NH3D {

struct Buffer {
    struct CreateInfo {
        size_t size; // in bytes
        BufferUsageFlags usage;
        BufferMemoryUsage memory;
        const byte* initialData; // optional
    };
};

}