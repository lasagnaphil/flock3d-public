#extension GL_EXT_nonuniform_qualifier : require

#ifndef LIGHTING_COMMON_GLSL
#define LIGHTING_COMMON_GLSL

const float PI = 3.14159265359;

#include "bindless_common.glsl"

struct PBRMaterial {
    vec3 albedo;
    float metallic;
    float roughness;
    float reflectance;
};

/*
// From LearnOpenGL

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a_sq = a * a;
    float N_dot_H = max(dot(N, H), 0.0);
    float N_dot_H_sq = N_dot_H * N_dot_H;

    float denom = N_dot_H_sq * (a_sq - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a2 / denom;
}

float geometry_schlick_ggx(float N_dot_V, float roughness) {
    float r = roughness + 1.0;
    float k = 0.125 * (r*r);
    return N_dot_V / (N_dot_V * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float N_dot_V = max(dot(N, V), 0.0);
    float N_dot_L = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(N_dot_V, roughness);
    float ggx1 = geometry_schlick_ggx(N_dot_L, roughness);

    return ggx1 * ggx2;
}
 */

// From Filament renderer
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

vec3 BSDF(PBRMaterial mat, vec2 uv, vec3 l, vec3 n, vec3 v) {
    vec3 h = normalize(v + l);
    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);


    float alpha = mat.roughness * mat.roughness;
    vec3 f0 = 0.16 * mat.reflectance * mat.reflectance * (1.0 - mat.metallic) + mat.albedo * mat.metallic;

    float D = D_GGX(NoH, alpha);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, alpha);

    // specular BRDF
    vec3 Fr = (D * V) * F;

    // diffuse BRDF
    vec3 diffuse = (1.0 - mat.metallic) * mat.albedo;
    vec3 Fd = diffuse * Fd_Lambert();

    return Fr + Fd;
}

vec3 calc_directional_light(DirectionalLight light, PBRMaterial mat, vec2 uv, vec3 n, vec3 v) {
    vec3 l = normalize(-light.direction);
    float NoL = clamp(dot(n, l), 0.0, 1.0);

    float illuminance = light.intensity * NoL;
    vec3 f = BSDF(mat, uv, l, n, v);
    vec3 luminance = light.color * f * illuminance;
    return luminance;
}

vec3 calc_point_light(PointLight light, PBRMaterial mat, vec2 uv, vec3 pos, vec3 n, vec3 v) {
    vec3 l = normalize(light.position - pos);
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    vec3 f = BSDF(mat, uv, l, n, v);
    float distance = length(light.position - pos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = light.color * attenuation;
    return f * radiance * NoL;
}

#endif