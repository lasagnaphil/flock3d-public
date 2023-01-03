#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) out vec4 fColor;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
layout(push_constant) uniform uPushConstant { 
    vec2 uScale; 
    vec2 uTranslate;
    uint tex_id;
} pc;

void main()
{
    fColor = In.Color * texture(textures[nonuniformEXT(pc.tex_id)], In.UV.st);
}