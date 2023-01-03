#version 450

#define GLSL
#include "../terrain_algo.cpp"

#include "terrain_common.glsl"

layout (set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    vec2 viewport_size;
} ubo;

#define MAX_OCTAVES 4

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec2 in_offset;

layout (location = 0) out vec3 frag_position;
layout (location = 1) out vec3 frag_normal;
layout (location = 2) out vec2 frag_uv;

void main() {
    frag_uv = in_offset + in_uv;
    vec2 pos = pc.out_scale_width * frag_uv;

    vec3 res = calc_terrain_with_gradient(pos,
        pc.in_scale, pc.octaves, pc.seed, pc.persistance, pc.lacunarity,
        pc.out_scale_width, pc.out_scale_height);

    vec4 world_position = vec4(pos.x, res.x, pos.y, 1.0);
    gl_Position = ubo.proj * (pc.view * world_position);

    frag_position = vec3(world_position);
    frag_normal = normalize(vec3(-res.y, 1, -res.z));
}