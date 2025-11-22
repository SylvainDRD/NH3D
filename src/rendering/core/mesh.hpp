#pragma once

#include <core/aabb.hpp>
#include <misc/types.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>

namespace NH3D {

struct Mesh {
    Handle<Buffer> vertexBuffer;
    Handle<Buffer> indexBuffer;

    AABB objectAABB;
};

}