#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

enum class MouseButton {
    Left = SDL_BUTTON_LMASK,
    Middle = SDL_BUTTON_MMASK,
    Right = SDL_BUTTON_RMASK,
};

class Window {
    NH3D_NO_COPY_MOVE(Window)
public:
    Window();

    ~Window();

    [[nodiscard]] VkSurfaceKHR createVkSurface(VkInstance instance) const;

    [[nodiscard]] uint32_t getWidth() const;

    [[nodiscard]] uint32_t getHeight() const;

    [[nodiscard]] bool pollEvents() const;

    [[nodiscard]] bool isKeyPressed(const SDL_Scancode key) const;

    [[nodiscard]] bool isMouseButtonPressed(const MouseButton button) const;

    [[nodiscard]] vec2 getMousePosition() const;

    [[nodiscard]] std::vector<const char*> requiredVulkanExtensions() const;

private:
    SDL_Window* _window;

    mutable SDL_MouseButtonFlags _mouseButtonState;

    mutable vec2 _mousePosition;

    uint32_t _width;

    uint32_t _height;
};

}