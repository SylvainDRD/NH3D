#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;

struct VertexInput 
{
	vec4 position;
	vec4 normal;
	vec4 uv;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer 
{
	VertexInput vertices[];
};

layout(push_constant) uniform PushConstants 
{
	VertexBuffer vertexBuffer;
} pushConstants;

void main() 
{
	// output the position of each vertex
	gl_Position = pushConstants.vertexBuffer.vertices[gl_VertexIndex].position;
	outColor = pushConstants.vertexBuffer.vertices[gl_VertexIndex].normal.xyz;
}