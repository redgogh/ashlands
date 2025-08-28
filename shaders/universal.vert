/**
 * -- Vertex Shader File --
 */
#version 450

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 color;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 outColor;

void main()
{
    gl_Position = pc.mvp * vec4(pos, 0.0f, 1.0f);
    outColor = color;
}