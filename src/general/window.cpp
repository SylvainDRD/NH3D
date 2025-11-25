#include "window.hpp"
#include <SDL3/SDL_vulkan.h>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace NH3D {

Window::Window()
{
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        NH3D_ABORT("SDL init failed");
    }

    _window = SDL_CreateWindow(NH3D_NAME, 1600, 800, 0);
    if (!_window) {
        NH3D_ABORT("Window creation failed");
    }

    int width, height;
    SDL_GetWindowSizeInPixels(_window, &width, &height);
    _width = static_cast<uint32_t>(width);
    _height = static_cast<uint32_t>(height);

    NH3D_LOG("Window creation completed");
}

Window::~Window()
{
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

[[nodiscard]] uint32_t Window::getWidth() const { return _width; }

[[nodiscard]] uint32_t Window::getHeight() const { return _height; }

[[nodiscard]] bool Window::pollEvents() const
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            // Only return early if quitting, or mouse state will be outdated
            return true;
        default:
            break;
        }
    }

    _mouseButtonState = SDL_GetMouseState(&_mousePosition.x, &_mousePosition.y);

    return false;
}

[[nodiscard]] bool Window::isKeyPressed(const SDL_Scancode key) const
{
    const bool* state = SDL_GetKeyboardState(nullptr);
    return state[static_cast<SDL_Scancode>(key)];
}

[[nodiscard]] bool Window::isMouseButtonPressed(const MouseButton button) const
{
    return _mouseButtonState & static_cast<SDL_MouseButtonFlags>(button);
}

[[nodiscard]] vec2 Window::getMousePosition() const { return _mousePosition; }

[[nodiscard]] std::vector<const char*> Window::requiredVulkanExtensions() const
{
    std::vector<const char*> exts {};

    uint32 count;
    const char* const* rawExts = SDL_Vulkan_GetInstanceExtensions(&count);

    for (int32 i = 0; i < count; ++i) {
        exts.emplace_back(rawExts[i]);

        NH3D_DEBUGLOG("SDL requiring extension \"" << rawExts[i] << "\"");
    }

    return exts;
}

[[nodiscard]] VkSurfaceKHR Window::createVkSurface(VkInstance instance) const
{
    VkSurfaceKHR surface;

    if (!SDL_Vulkan_CreateSurface(_window, instance, nullptr, &surface)) {
        NH3D_ABORT_VK("Failed to create the window surface");
    }

    return surface;
}
}