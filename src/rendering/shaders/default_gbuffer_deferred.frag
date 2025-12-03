#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "structs.inc.glsl"

layout(set = 0, binding = 0) uniform sampler linearSampler;
layout(set = 0, binding = 1) uniform texture2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in Material inMaterial;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outAlbedo; // TODO: only store material ID?

void main()
{
    if (inMaterial.albedoTexture != ~0u) {
        outAlbedo = texture(sampler2D(textures[inMaterial.albedoTexture], linearSampler), inUV).rgb;
    }
    else {
        // Default bright magenta for missing textures
        outAlbedo = vec3(1.0, 0.0, 1.0);
    }

    outNormal = inNormal; // TODO: octahedral encoding
}
