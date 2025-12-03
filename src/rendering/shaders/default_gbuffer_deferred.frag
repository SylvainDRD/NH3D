#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "structs.inc.glsl"

layout(set = 0, binding = 0) uniform sampler linearSampler;
layout(set = 0, binding = 1) uniform texture2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in Material inMaterial;

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec3 outAlbedo;

// Reference: https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
vec3 signedOctEncode(vec3 n)
{
    vec3 normal;
    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    normal.y = n.y * 0.5 + 0.5;
    normal.x = n.x * 0.5 + normal.y;
    normal.y = n.x * -0.5 + normal.y;

    normal.z = step(0.0, n.z);
    return normal;
}

// In scenarios with a lot of overlapping objects, albedo texture lookups become costly
// In a realistic scenario, this is probably not significant enough to justify a new GBuffer target with UVs and ddx/dyy
// Although, in my test scene, I was expecting early Z rejection to help a lot more, this might still be worth investigating
void main()
{
    vec3 encodedNormal = signedOctEncode(normalize(inNormal));
    outNormal = vec4(encodedNormal.xy, 0.0 /* unused for now*/ , encodedNormal.z);

    if (inMaterial.albedoTexture != ~0u) {
        outAlbedo = texture(sampler2D(textures[inMaterial.albedoTexture], linearSampler), inUV).rgb;
    }
    else {
        // Default bright magenta for missing textures
        outAlbedo = vec3(1.0, 0.0, 1.0);
    }
}
