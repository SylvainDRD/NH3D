#version 460

#include "structs.inc.glsl"

layout(set = 1, binding = 0, scalar) readonly buffer DrawRecordBuffer {
    DrawRecord drawRecords[];
};

layout(push_constant) uniform ProjectionMatrix
{
    mat4 matrix;
} projection;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out flat Material outMaterial; // TODO: benchmark bindless materials

vec3 normalEncode(vec3 n) {
    return n * 0.5 + 0.5;
}

void main()
{
    DrawRecord drawRecord = drawRecords[gl_DrawID];

    uint index = drawRecord.indexBuffer.indices[gl_VertexIndex];
    vec3 viewPosition = drawRecord.modelViewMatrix * vec4(drawRecord.vertexBuffer.vertices[index].position, 1.0);
    // TODO: uncomment
    // gl_Position = projection.matrix * vec4(viewPosition, 1.0);
    // TODO: delete
    float y = drawRecord.vertexBuffer.vertices[index].position.y * -1.0;
    gl_Position = vec4(drawRecord.vertexBuffer.vertices[index].position.x, y, drawRecord.vertexBuffer.vertices[index].position.z, 1.0);

    vec3 viewNormal = drawRecord.modelViewMatrix * vec4(drawRecord.vertexBuffer.vertices[index].normal, 0.0);
    outNormal = normalEncode((projection.matrix * vec4(viewNormal, 0.0)).xyz); // TODO: octahedron encode https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    outUV = drawRecord.vertexBuffer.vertices[index].uv;
    outMaterial = drawRecord.material;
}
