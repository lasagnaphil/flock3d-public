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
    out_frag_color = push_constants.color;
}
