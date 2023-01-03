#version 450

#ifdef LINE_AA
layout (location = 0) in vec2 frag_linecenter;
#endif

layout (location = 0) out vec4 out_frag_color;

layout (push_constant) uniform PushConstants {
    mat4 projview;
    vec4 color;
    vec2 viewport_size;
    float prim_width;
    float blend_factor;
} push_constants;

void main() {
    vec4 col = push_constants.color;
#ifdef LINE_AA
    vec2 coord = frag_linecenter - gl_FragCoord.xy;
    float d_sq = dot(coord, coord);
    float w = push_constants.prim_width;
    if (d_sq > w*w)
        col.w = 0;
    else
        col.w *= pow((w - sqrt(d_sq))/w, push_constants.blend_factor);
#endif
    out_frag_color = col;
}