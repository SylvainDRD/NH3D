#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;

struct VertexInput 
{
	vec3 position;
	vec3 normal;
	vec2 uv;
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
	gl_Position = vec4(pushConstants.vertexBuffer.vertices[gl_VertexIndex].position, 1.0f);
	outColor = pushConstants.vertexBuffer.vertices[gl_VertexIndex].normal;
}