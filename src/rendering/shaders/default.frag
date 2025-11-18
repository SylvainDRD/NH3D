#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 0, binding = 0) uniform sampler2D linearSampler;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = vec4(inColor, 1.0f);
}