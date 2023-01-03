#ifndef CAMERA_COMMON_GLSL
#define CAMERA_COMMON_GLSL

layout(push_constant) uniform PushConstants {
    mat4 view_model;
    vec4 color;
    vec3 cam_pos;
    uint mat_id;
} pc;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    vec2 viewport_size;
} ubo;

#endif