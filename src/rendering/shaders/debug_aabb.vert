#version 460

#include "structs.inc.glsl"
#include "common.inc.glsl"

layout(set = 0, binding = 0, scalar) readonly buffer ObjectTransformsBuffer {
    TransformData transforms[];
} objectTransforms;

layout(set = 0, binding = 1, scalar) readonly buffer ObjectAABBsBuffer {
    AABB aabb[];
} objectAABBs;

layout(push_constant, scalar) uniform PushConstants {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} pushConstants;

// Render a wireframe AABB for debugging purposes
// Not subjected to culling but should be lightweight enough with instance drawing

void main()
{
    // Counter clockwise winding order
    const vec3 AABBVertices[8] = vec3[](
            vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0), // Front face
            vec3(0, 0, 1), vec3(1, 0, 1), vec3(1, 1, 1), vec3(0, 1, 1) // Back face
        );
    const uint indices[24] = uint[](
            0, 1, 1, 2, 2, 3, 3, 0, // Front face
            4, 5, 5, 6, 6, 7, 7, 4, // Back face
            0, 4, 1, 5, 2, 6, 3, 7 // Side edges
        );

    TransformData transform = objectTransforms.transforms[gl_InstanceIndex];
    AABB objectAABB = objectAABBs.aabb[gl_InstanceIndex];

    mat4 viewModelMatrix = pushConstants.viewMatrix * computeTransform(transform);
    AABB viewAABB = transformAABB(viewModelMatrix, objectAABB);

    vec3 aabbCorner = AABBVertices[indices[gl_VertexIndex]];
    vec3 viewVertex = mix(viewAABB.min, viewAABB.max, aabbCorner);
    gl_Position = pushConstants.projectionMatrix * vec4(viewVertex, 1.0);
}
