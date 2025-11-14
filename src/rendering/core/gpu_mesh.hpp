#pragma once

#include <misc/types.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>

namespace NH3D {

struct GPUMesh {
    Handle<Buffer> vertexBuffer;
    Handle<Buffer> indexBuffer;
};

}