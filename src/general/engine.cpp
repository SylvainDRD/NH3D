#include "engine.hpp"
#include <memory>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/scene.hpp>
#include <vector>

namespace NH3D {

Engine::Engine()
    : _window {}
    , _rhi { std::make_unique<VulkanRHI>(_window) }
    , _mainScene { *_rhi.get() }
    , _resourceMapper { std::make_unique<ResourceMapper>() }
{
}

Engine::~Engine() { }

IRHI& Engine::getRHI() const { return *_rhi; }

Scene& Engine::getMainScene() const { return _mainScene; }

ResourceMapper& Engine::getResourceMapper() const { return *_resourceMapper; }

bool Engine::update()
{
    if (!_window.windowClosing()) {
        _rhi->render(_mainScene);
        _window.update();
        return true;
    }
    return false;
}

}