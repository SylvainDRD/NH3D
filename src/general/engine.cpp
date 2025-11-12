#include "engine.hpp"
#include <memory>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <scene/scene.hpp>

namespace NH3D {

Engine::Engine()
    : _window {}
{
    _rhi = std::make_unique<VulkanRHI>(_window);
}

Engine::~Engine() { }

void Engine::run()
{
    Scene scene { *_rhi.get() };
    while (!_window.windowClosing()) {
        _rhi->render(scene);
        _window.update();
    }
}

}