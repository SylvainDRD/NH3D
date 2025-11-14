#pragma once

#include <general/resource_mapper.hpp>
#include <general/window.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/rhi.hpp>
#include <scene/scene.hpp>

namespace NH3D {

class Engine {
    NH3D_NO_COPY_MOVE(Engine)
public:
    Engine();

    ~Engine();

    [[nodiscard]] IRHI& getRHI() const;

    [[nodiscard]] Scene& getMainScene() const;

    ResourceMapper& getResourceMapper() const;

    [[nodiscard]] bool update();

private:
    Window _window;

    Uptr<IRHI> _rhi;

    mutable Scene _mainScene;

    Uptr<ResourceMapper> _resourceMapper;
};

}