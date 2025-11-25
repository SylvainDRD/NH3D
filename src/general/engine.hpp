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

    [[nodiscard]] Window& getWindow();

    [[nodiscard]] IRHI& getRHI();

    [[nodiscard]] Scene& getMainScene();

    ResourceMapper& getResourceMapper();

    [[nodiscard]] bool update();

    // In seconds, only valid after the first update() call
    [[nodiscard]] float deltaTime() const;

private:
    Window _window;

    Uptr<IRHI> _rhi;

    Scene _mainScene;

    Uptr<ResourceMapper> _resourceMapper;

    std::chrono::time_point<std::chrono::high_resolution_clock> _lastFrameStartTime {};

    // in seconds
    float _deltaTime = NAN;
};

}