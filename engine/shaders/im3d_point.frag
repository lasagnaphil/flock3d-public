#version 450

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
#ifdef POINT_ROUNDED
    vec2 coord = 2.0 * gl_PointCoord - 1.0;
    float d_sq = dot(coord, coord);
    if (d_sq > 1.0) {
        col.w = 0;
    }
#ifdef POINT_AA
    else {
        col.w *= pow(1.0 - sqrt(d_sq), push_constants.blend_factor);
    }
#endif
#endif
    out_frag_color = col;
}
