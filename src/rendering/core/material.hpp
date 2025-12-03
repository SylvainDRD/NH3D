#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/texture.hpp>

namespace NH3D {

struct Material {
    Handle<Texture> albedoTexture = InvalidHandle<Texture>; // invalid handle if no texture
};

}
