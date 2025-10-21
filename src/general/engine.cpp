#include "engine.hpp"
#include <memory>
#include <rendering/render_graph/render_graph.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <scene/ecs/entity_manager.hpp>
#include <scene/scene.hpp>

namespace NH3D {

Engine::Engine()
    : _window {}
{
    _rhi = std::make_unique<VulkanRHI>(_window);

    EntityManager::prealloc();
}

Engine::~Engine() { }

void Engine::run()
{
    while (!_window.windowClosing()) {
        _rhi->render(RenderGraph { Scene("TODO :)") });
        _window.update();
    }
}

}