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

using EntityTag = uint16;

namespace EntityTags {

    // TODO: serialize as strings to be resilient to tag changes
    enum Tags : EntityTag {
        Visible = 1 << 0,
        Enabled = 1 << 1,
        CastShadows = 1 << 2,
        MainCamera = 1 << 3,
        Default = Visible | Enabled | CastShadows,
        MAX = 1 << 15
    };

    [[nodiscard]] inline static bool checkTag(const EntityTag entityTag, const EntityTag tag) { return (entityTag & tag) == tag; }

}

}