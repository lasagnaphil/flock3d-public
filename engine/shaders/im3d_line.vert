#version 450

layout (location = 0) in vec3 in_position;

#ifdef LINE_AA
layout (location = 0) out vec2 frag_linecenter;
#endif

layout (push_constant) uniform PushConstants {
    mat4 projview;
    vec4 color;
    vec2 viewport_size;
    float prim_width;
    float blend_factor;
} push_constants;

void main()
{
    vec4 pos = push_constants.projview * vec4(in_position, 1.0);
    gl_Position = pos;
#ifdef LINE_AA
    frag_linecenter = 0.5 * (pos.xy + vec2(1, 1)) * push_constants.viewport_size;
#endif
}
