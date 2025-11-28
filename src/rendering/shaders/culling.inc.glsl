#ifndef CULLING_INC_GLSL
#define CULLING_INC_GLSL

struct FrustumPlanes {
    vec2 left;
    vec2 right;
    vec2 bottom;
    vec2 top;
};

struct CullingParameters {
    mat4 viewMatrix;
    FrustumPlanes frustum;
    uint objectCount;
};

// It's fine to put buffers here because I'm not expecting much instancing (if any)
struct MeshData {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Material material;
};

struct RenderData {
    MeshData mesh;
    uint indexCount;
};

// glsl doesn't allow for unsized arrays as function parameters, macro workaround
#define isVisibleFlagSet(flags, index) ((flags[index >> 5] & (1 << (index & 31))) != 0)

bool inFrustum(AABB viewAABB, CullingParameters cullingParams) {
    // Near
    if (viewAABB.max.z < 0.0) {
        return false;
    }

    // Components equal to zero were dropped

    // Left (plane y == 0)
    if (dot(viewAABB.max.xz, cullingParams.frustum.left) < 0) {
        return false;
    }

    // Right (plane y == 0)
    if (dot(vec2(viewAABB.min.x, viewAABB.max.z), cullingParams.frustum.right) < 0) {
        return false;
    }

    // Bottom (plane x == 0)
    if (dot(viewAABB.max.yz, cullingParams.frustum.bottom) < 0) {
        return false;
    }

    // Top (plane x == 0)
    if (dot(vec2(viewAABB.min.y, viewAABB.max.z), cullingParams.frustum.top) < 0) {
        return false;
    }

    return true;
}

#endif // CULLING_INC_GLSL
