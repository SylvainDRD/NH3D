#version 460

#include "structs.inc.glsl"

layout(set = 1, binding = 0, scalar) readonly buffer DrawRecordBuffer {
    DrawRecord drawRecords[];
};

layout(push_constant) uniform ViewProjectionMatrix
{
    mat4 matrix;
} projection;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out flat Material outMaterial; // TODO: benchmark bindless materials

void main()
{
    // TODO: apply transform

    DrawRecord drawRecord = drawRecords[gl_DrawID];

    uint index = drawRecord.indexBuffer.indices[gl_VertexIndex];
    gl_Position = vec4(drawRecord.vertexBuffer.vertices[index].position.xyz, 1.0);

    outNormal = drawRecord.vertexBuffer.vertices[index].normal;
    outUV = drawRecord.vertexBuffer.vertices[index].uv;
    outMaterial = drawRecord.material;
}
