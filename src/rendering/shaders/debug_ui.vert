#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// Directly inspired from Shascha Willems' Vulkan examples
// https://github.com/SaschaWillems/Vulkan

struct ImGuiVertex {
    vec2 pos;
    vec2 uv;
    vec4 col;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    ImGuiVertex vertices[];
};

layout(push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
    VertexBuffer vertexBuffer;
} pushConstants;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

void main()
{
    ImGuiVertex vertex = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    outUV = vertex.uv;
    outColor = vertex.col;
    gl_Position = vec4(vertex.pos * pushConstants.scale + pushConstants.translate, 0.0, 1.0);
}
