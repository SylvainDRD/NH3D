#pragma once

#include <misc/types.hpp>

namespace NH3D {

struct TransformComponent {
    quat rotation;
    vec3 position;
    vec3 scale;
};

}