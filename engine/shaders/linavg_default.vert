#version 450 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 col;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    vec2 viewport_size;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D textures[];

layout (location = 0) out struct {
    vec4 fCol;
    vec2 fUV;
} Out;

void main()
{
    Out.fCol = col;
    Out.fUV = uv;
    gl_Position = ubo.proj * vec4(pos.x, pos.y, 0.0f, 1.0);
}