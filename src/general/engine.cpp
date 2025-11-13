#include "engine.hpp"
#include <memory>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
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

    // Debug only
    scene.create(RenderComponent { *_rhi.get(),
        {
            { .position = vec4 { -1.f, 1.f, 0.f, 1.f }, .normal = vec4 { 1.0f, 0.0f, 0.0f, 0.0f }, .uv = vec2 { 1.0f, 0.0f } },
            { .position = vec4 { 1.f, 1.f, 0.f, 1.f }, .normal = vec4 { 0.0f, 1.0f, 0.0f, 0.0f }, .uv = vec2 { 0.0f, 0.0f } },
            { .position = vec4 { 0.f, -1.f, 0.f, 1.f }, .normal = vec4 { 0.0f, 0.0f, 1.0f, 0.0f }, .uv = vec2 { 0.5f, 1.0f } },
        },
        std::vector<uint32_t> { 0, 1, 2 } });

    while (!_window.windowClosing()) {
        _rhi->render(scene);
        _window.update();
    }
}

}