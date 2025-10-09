#include "engine.hpp"
#include <memory>
#include <rendering/render_graph/render_graph.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>

namespace NH3D {

Engine::Engine()
    : _window {}
{
    _rhi = std::make_unique<VulkanRHI>(_window);
}

Engine::~Engine() { }

void Engine::run()
{
    while (!_window.windowClosing()) {
        _rhi->render(RenderGraph {});
        _window.update();
    }
}

} 