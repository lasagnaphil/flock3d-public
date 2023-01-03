#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

#include "linavg_push_constants_frag.glsl"

layout (set = 1, binding = 0) uniform sampler2D textures[];

layout (location = 0) out vec4 fragColor;

layout (location = 0) in struct {
    vec2 fUV;
    vec4 fCol;
} In;

void main()
{
    vec4 col = texture(textures[nonuniformEXT(pc.diffuse)], In.fUV * pc.tiling + pc.offset);
    fragColor = vec4(col.rgb, pc.isAABuffer == 1 ? In.fCol.a : col.a) * pc.tint; 
}