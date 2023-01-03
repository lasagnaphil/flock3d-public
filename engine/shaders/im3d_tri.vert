#version 450

layout (location = 0) in vec3 in_position;

layout (push_constant) uniform PushConstants {
    mat4 projview;
    vec4 color;
    vec2 viewport_size;
    float prim_width;
    float blend_factor;
} push_constants;

void main()
{
    gl_Position = push_constants.projview * vec4(in_position, 1.0);
}
