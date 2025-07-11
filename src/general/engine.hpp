#pragma once

#include <general/window.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>

namespace NH3D {

class VulkanRHI;

class Engine {
    NH3D_NO_COPY_MOVE(Engine)
public:
    Engine();

    ~Engine();

    void run();

private:
    Window _window;

    Uptr<VulkanRHI> _rhi;
};

} 