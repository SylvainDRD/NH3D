#version 460

// Directly inspired from Shascha Willems' Vulkan examples
// https://github.com/SaschaWillems/Vulkan

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(push_constant, std430) uniform PushConstants {
    vec2 scale;
    vec2 translate;
} pushConstants;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

void main()
{
    outUV = inUV;
    outColor = inColor;
    gl_Position = vec4(inPosition * pushConstants.scale + pushConstants.translate, 0.0, 1.0);
}
