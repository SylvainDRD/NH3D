#version 460
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
	uint albedoTexture;
	vec3 albedo;
};

struct DrawRecord {
	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	Material material;
	mat4x3 modelMatrix;
};

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