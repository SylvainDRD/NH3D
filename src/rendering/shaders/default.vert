#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "structs.inc"

layout (set = 1, binding = 0, scalar) readonly buffer DrawRecordBuffer {
	DrawRecord drawRecords[];
};

layout(push_constant) uniform ViewProjectionMatrix 
{
	mat4 matrix;
} projection;

layout (location = 0) out vec3 outColor;

void main() 
{
	// TODO: apply transform

	DrawRecord drawRecord = drawRecords[gl_DrawID];

	// output the position of each vertex
	uint index = drawRecord.indexBuffer.indices[gl_VertexIndex];
	gl_Position = vec4(drawRecord.vertexBuffer.vertices[index].position.xyz, 1.0);
	outColor = drawRecord.material.albedo;
}