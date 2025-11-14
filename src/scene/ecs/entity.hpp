#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>

namespace NH3D {

using Entity = uint32;
static constexpr Entity InvalidEntity = NH3D_MAX_T(Entity);

using ComponentMask = uint32;

namespace ComponentMasks {

    [[nodiscard]] inline static bool checkComponents(const ComponentMask entityMask, const ComponentMask componentMask)
    {
        return (entityMask & componentMask) == componentMask;
    }

}

}