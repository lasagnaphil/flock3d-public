#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec4 fragColor;

layout (location = 0) in struct {
    vec4 fCol;
    vec2 fUV;
} In;

void main()
{
    fragColor = In.fCol;
}