#version 450

#include "camera_common.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

layout (location = 0) out vec3 frag_position;
layout (location = 1) out vec3 frag_normal;
layout (location = 2) out vec2 frag_texcoord;

void main() {
    gl_Position = ubo.proj * pc.view_model * vec4(in_position, 1);
    frag_position = in_position;
    frag_normal = in_normal;
    frag_texcoord = in_texcoord;
}