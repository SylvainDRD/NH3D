#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/texture.hpp>

namespace NH3D {

struct Material {
    color3 albedo;
    Handle<Texture> albedoTexture; // invalid handle if no texture
};

}