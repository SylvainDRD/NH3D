#ifndef COMMON_INC_GLSL
#define COMMON_INC_GLSL

#include "structs.inc.glsl"

mat4 computeTransform(TransformData t) {
    vec4 q = 1.4142135623730951 * t.rotation; // multiply by sqrt(2) to remove * 2

    float qx2 = q.x * q.x;
    float qy2 = q.y * q.y;
    float qz2 = q.z * q.z;
    float qw2 = q.w * q.w;
    float qxy = q.x * q.y;
    float qxz = q.x * q.z;
    float qxw = q.x * q.w;
    float qyz = q.y * q.z;
    float qyw = q.y * q.w;
    float qzw = q.z * q.w;

    mat4 transform;
    transform[0][0] = 1.0 - qy2 - qz2;
    transform[0][1] = qxy + qzw;
    transform[0][2] = qxz - qyw;
    transform[0][3] = 0.0;
    transform[1][0] = qxy - qzw;
    transform[1][1] = 1.0 - qx2 - qz2;
    transform[1][2] = qyz + qxw;
    transform[1][3] = 0.0;
    transform[2][0] = qxz + qyw;
    transform[2][1] = qyz - qxw;
    transform[2][2] = 1.0 - qx2 - qy2;
    transform[2][3] = 0.0;

    transform[0] *= t.scale.x;
    transform[1] *= t.scale.y;
    transform[2] *= t.scale.z;

    transform[3] = vec4(t.translation, 1.0);

    return transform;
}

AABB transformAABB(mat4 transform, AABB objectAABB) {
    // Compute world AABB from local AABB, see Graphics Gems - "Transforming Axis-Aligned Bounding Boxes"
    // It's just decomposing the matrix multiplication to avoid doing 8 corner transformations and a lot of min/max ops
    // Not too sure how efficient this is on GPU TBH
    vec3 nmin, nmax;
    nmin = nmax = transform[3].xyz; // Translation part
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float a = transform[j][i] * objectAABB.min[j];
            float b = transform[j][i] * objectAABB.max[j];
            if (a < b) {
                nmin[i] += a;
                nmax[i] += b;
            } else {
                nmin[i] += b;
                nmax[i] += a;
            }
        }
    }

    // vec3 points[8] = {
    //         objectAABB.min,
    //         objectAABB.max,
    //         vec3(objectAABB.min.x, objectAABB.min.y, objectAABB.max.z),
    //         vec3(objectAABB.min.x, objectAABB.max.y, objectAABB.min.z),
    //         vec3(objectAABB.min.x, objectAABB.max.y, objectAABB.max.z),
    //         vec3(objectAABB.max.x, objectAABB.min.y, objectAABB.min.z),
    //         vec3(objectAABB.max.x, objectAABB.min.y, objectAABB.max.z),
    //         vec3(objectAABB.max.x, objectAABB.max.y, objectAABB.min.z),
    //     };

    // vec3 nmin, nmax;
    // nmin = nmax = (transform * vec4(points[0], 1.0)).xyz;
    // for (int i = 1; i < 8; ++i) {
    //     vec3 point = (transform * vec4(points[i], 1.0)).xyz;
    //     nmin = min(nmin, point);
    //     nmax = max(nmax, point);
    // }

    return AABB(nmin, nmax);
}

#endif // COMMON_INC_GLSL
