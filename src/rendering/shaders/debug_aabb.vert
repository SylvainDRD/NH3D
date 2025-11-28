#version 460

#include "structs.inc.glsl"
#include "common.inc.glsl"

layout(set = 0, binding = 0) readonly buffer ObjectAABBsBuffer {
    AABB aabb[];
} objectAABBs;

layout(set = 0, binding = 1) readonly buffer ObjectTransformsBuffer {
    TransformData transforms[];
} objectTransforms;

layout(push_constant) uniform PushConstants {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} pushConstants;

void main()
{
    const vec3 AABBVertices[8] = vec3[](
            vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0),
            vec3(0, 0, 1), vec3(1, 0, 1), vec3(1, 1, 1), vec3(0, 1, 1)
        );
    const uint indices[24] = uint[](
            0, 1,
            1, 2,
            2, 3,
            3, 0,
            4, 5,
            5, 6,
            6, 7,
            7, 4,
            0, 4,
            1, 5,
            2, 6,
            3, 7
        );

    TransformData transform = objectTransforms.transforms[gl_InstanceIndex];
    AABB objectAABB = objectAABBs.aabb[gl_InstanceIndex];

    mat4 viewModelMatrix = pushConstants.viewMatrix * computeTransform(transform);
    AABB viewAABB = transformAABB(viewModelMatrix, objectAABB);

    vec3 aabbCorner = AABBVertices[indices[gl_VertexIndex]];
    vec3 viewVertex = mix(viewAABB.min, viewAABB.max, aabbCorner);
    gl_Position = pushConstants.projectionMatrix * vec4(viewVertex, 1.0);
}
