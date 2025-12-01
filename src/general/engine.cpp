#include "engine.hpp"
#include <memory>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/scene.hpp>

namespace NH3D {

Engine::Engine()
    : _window {}
    , _rhi { std::make_unique<VulkanRHI>(_window) }
    , _mainScene { *_rhi.get() }
    , _resourceMapper { std::make_unique<ResourceMapper>() }
{
}

Engine::~Engine() { }

Window& Engine::getWindow() { return _window; }

IRHI& Engine::getRHI() { return *_rhi; }

Scene& Engine::getMainScene() { return _mainScene; }

ResourceMapper& Engine::getResourceMapper() { return *_resourceMapper; }

bool Engine::update()
{
    const auto currentTime = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float, std::milli> deltaTime = currentTime - _lastFrameStartTime;
    _deltaTime = deltaTime.count() / 1000.0;

    _lastFrameStartTime = std::chrono::high_resolution_clock::now();
    if (!_window.pollEvents()) {
        _rhi->render(_mainScene);
        return true;
    }
    return false;
}

float Engine::deltaTime() const { return _deltaTime; }

} // namespace NH3D