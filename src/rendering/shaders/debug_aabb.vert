#version 460

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 projectionMatrix;
} pushConstants;

void main()
{
    gl_Position = pushConstants.projectionMatrix * vec4(inPosition, 1.0);
}
