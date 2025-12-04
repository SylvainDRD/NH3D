#ifndef STRUCTS_INC_GLSL
#define STRUCTS_INC_GLSL

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

struct VertexInput
{
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer
{
    VertexInput vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer
{
    uint16_t indices[];
};

struct Material {
    uint albedoTexture;
};

struct DrawRecord {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Material material;
    mat4x3 modelViewMatrix;
};

struct TransformData {
    vec4 rotation;
    vec3 translation;
    vec3 scale;
};

struct AABB {
    vec3 min;
    vec3 max;
};

#endif // STRUCTS_INC_GLSL
