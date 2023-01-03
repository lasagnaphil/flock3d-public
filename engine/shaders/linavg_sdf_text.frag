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
    float distance = texture(textures[nonuniformEXT(pc.diffuse)], In.fUV).r;
    float alpha = smoothstep(pc.thickness - pc.softness, pc.thickness + pc.softness, distance);
    vec3 baseColor = In.fCol.rgb;
    if (pc.outlineEnabled == 1) {
        float border = smoothstep(pc.thickness + pc.outlineThickness - pc.softness, pc.thickness + pc.outlineThickness + pc.softness, distance);
        baseColor = mix(pc.outlineColor, In.fCol, border).rgb;
    } 
    fragColor = vec4(baseColor, pc.flipAlpha == 1 ? 1.0f - alpha : alpha);
}