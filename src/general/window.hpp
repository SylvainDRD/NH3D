#pragma once

#include <misc/utils.hpp>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

namespace NH3D {

class Window {
    NH3D_NO_COPY_MOVE(Window)
public:
    Window();

    ~Window();

    [[nodiscard]] VkSurfaceKHR createVkSurface(VkInstance instance) const;

    [[nodiscard]] inline uint32_t getWidth() const { return _width; }

    [[nodiscard]] inline uint32_t getHeight() const { return _height; }

    inline void update() { glfwPollEvents(); }

    [[nodiscard]] inline bool windowClosing() const { return glfwWindowShouldClose(_window); }

    [[nodiscard]] std::vector<const char*> requiredVulkanExtensions() const;

private:
    GLFWwindow* _window;

    uint32_t _width;

    uint32_t _height;
};

} 