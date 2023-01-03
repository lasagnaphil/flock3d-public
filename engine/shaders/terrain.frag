#version 450

#include "terrain_common.glsl"
#include "bindless_common.glsl"
#include "lighting_common.glsl"

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 frag_normal;
layout (location = 2) in vec2 frag_uv;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 n = normalize(frag_normal);
    vec3 cam_position = vec3(pc.view[3]);
    vec3 v = normalize(cam_position - frag_position);

    // TODO: Don't hardcode these values and use existing material system instead
    PBRMaterial pbr_mat;
    pbr_mat.albedo = vec3(0.53, 0.36, 0.24);
    pbr_mat.metallic = 0.0;
    pbr_mat.roughness = 0.99;
    pbr_mat.reflectance = 1.0;

    vec3 color = vec3(0.0);
    color += calc_directional_light(lighting.dir, pbr_mat, frag_uv, n, v);
    for (int i = 0; i < lighting.num_points; i++) {
        color += calc_point_light(lighting.point[i], pbr_mat, frag_uv, frag_position, n, v);
    }
    color += vec3(0.03) * pbr_mat.albedo * pbr_mat.reflectance;
    // out_color = vec4(color, albedo.a);
    out_color = vec4(color, 1.0);

    vec2 frag_uv_mod = mod(frag_uv, 1.0);
    if (abs(frag_uv_mod.x) < 5e-3 || abs(frag_uv_mod.y) < 5e-3) {
        out_color = vec4(1);
    }
}
