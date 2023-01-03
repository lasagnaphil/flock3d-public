#version 450
#extension GL_EXT_nonuniform_qualifier : require

#define DEFAULT_ALBEDO_TEX 0
#define DEFAULT_MR_TEX 1
#define DEFAULT_AO_TEX 2

#include "camera_common.glsl"
#include "bindless_common.glsl"
#include "lighting_common.glsl"

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 frag_normal;
layout (location = 2) in vec2 frag_texcoord;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(frag_normal);
    vec3 v = normalize(pc.cam_pos - frag_position);

    Material mat = material_buffer.materials[nonuniformEXT(pc.mat_id)];

    PBRMaterial pbr_mat;
    vec4 albedo = mat.albedo * texture(textures[nonuniformEXT(mat.albedo_tex_id)], frag_texcoord);
    pbr_mat.albedo = albedo.rgb;
    vec2 mr = texture(textures[nonuniformEXT(mat.metallic_roughness_tex_id)], frag_texcoord).rg;
    pbr_mat.metallic = mat.metallic * mr.x;
    pbr_mat.roughness = mat.roughness * mr.y;
    pbr_mat.reflectance = mat.reflectance * texture(textures[nonuniformEXT(mat.reflectance_tex_id)], frag_texcoord).r;

    vec3 color = vec3(0.0);
    color += calc_directional_light(lighting.dir, pbr_mat, frag_texcoord, n, v);
    for (int i = 0; i < lighting.num_points; i++) {
        color += calc_point_light(lighting.point[i], pbr_mat, frag_texcoord, frag_position, n, v);
    }

    color += vec3(0.03) * pbr_mat.albedo * pbr_mat.reflectance;

    out_color = vec4(color, albedo.a);
}