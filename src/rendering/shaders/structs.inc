
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