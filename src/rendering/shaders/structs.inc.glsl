#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

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
    uint indices[];
};

struct Material {
    vec3 albedo;
    uint albedoTexture;
};

struct DrawRecord {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Material material;
    mat4x3 modelViewMatrix;
};
