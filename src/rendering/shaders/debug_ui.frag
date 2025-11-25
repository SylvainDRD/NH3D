#version 460

// Directly taken from Shascha Willems' Vulkan examples
// https://github.com/SaschaWillems/Vulkan

layout(set = 0, binding = 0) uniform sampler2D fontSampler;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = inColor * texture(fontSampler, inUV);
    outColor.rgb = pow(outColor.rgb, vec3(2.2)); // Reverse sRGB gamma correction, a bit hacky but heh
}
