#ifndef TERRAIN_COMMON_GLSL
#define TERRAIN_COMMON_GLSL

layout (push_constant) uniform PushConstants {
    mat4 view;

    float in_scale;
    int octaves;
    uint seed;
    uint mat_id;

    float persistance;
    float lacunarity;

    float out_scale_width;
    float out_scale_height;
} pc;

#endif