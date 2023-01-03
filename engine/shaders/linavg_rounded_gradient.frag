#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

#include "linavg_push_constants_frag.glsl"

layout (location = 0) out vec4 fragColor;
layout (location = 0) in struct {
	vec2 fUV;
	vec4 fCol;
} In;

void main()
{
	if (pc.gradientType == 0) {
		vec4 col = mix(pc.startColor, pc.endColor, In.fUV.x);
		fragColor = vec4(col.rgb, pc.isAABuffer == 1 ? In.fCol.a : col.a); 
	}
	else if (pc.gradientType == 1){
		vec4 col = mix(pc.startColor, pc.endColor, In.fUV.y);
		fragColor = vec4(col.rgb, pc.isAABuffer == 1 ? In.fCol.a : col.a); 
	}
	else if (pc.gradientType == 2) 
	{
		vec2 uv = In.fUV - vec2(0.5, 0.5);
		float dist = length(uv * pc.radialSize);
		vec4 col = mix(pc.startColor, pc.endColor, dist);
		fragColor = vec4(col.rgb, pc.isAABuffer == 1 ? In.fCol.a : col.a); 
	}
	else if (pc.gradientType == 3) 
	{
		float dist = length(In.fUV * pc.radialSize);
		vec4 col = mix(pc.startColor, pc.endColor, dist);
		fragColor = vec4(col.rgb, pc.isAABuffer == 1 ? In.fCol.a : col.a); 
	}
}