#ifndef BINDLESS_COMMON_GLSL
#define BINDLESS_COMMON_GLSL

#define MAX_POINT_LIGHTS 256

struct Material {
    vec4 albedo;
    float metallic;
    float roughness;
    float reflectance;
    float _padding;

    uint albedo_tex_id;
    uint metallic_roughness_tex_id;
    uint reflectance_tex_id;
};

struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
    float _padding2;
};

struct PointLight {
    vec3 position;
    float _padding1;
    vec3 color;
    float _padding2;
};

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(set = 2, binding = 0) readonly buffer MaterialBuffer { Material materials[]; } material_buffer;

layout(set = 3, binding = 0) readonly buffer LightingBuffer { 
    DirectionalLight dir;
    PointLight point[MAX_POINT_LIGHTS];
    uint num_points;
} lighting;

#endif