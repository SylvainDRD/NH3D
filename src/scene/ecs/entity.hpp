#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>

namespace NH3D {

using Entity = uint32;
static constexpr Entity InvalidEntity = NH3D_MAX_T(Entity);

using ComponentMask = uint32;

}