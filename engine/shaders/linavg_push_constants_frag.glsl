#ifndef LINAVG_PUSH_CONSTANTS_GLSL
#define LINAVG_PUSH_CONSTANTS_GLSL

// Packing all the constants used in all fragment shaders into a 128-byte struct (minimum push constant size)

layout (push_constant) uniform uPushConstant {
    // linavg_textured.frag (32 bytes)
	vec2 tiling;
	vec2 offset;
	vec4 tint;

	// linavg_sdf_text.frag (48 bytes)
    float softness; 
    float thickness; 
    int outlineEnabled; 
    int useOutlineOffset; 
    vec2 outlineOffset; 
    float outlineThickness; 
    int flipAlpha;
    vec4 outlineColor; 

    // linavg_rounded_gradient.frag (40 bytes)
	vec4 startColor;
	vec4 endColor;
	int  gradientType;
	float radialSize;

    // shared (8 bytes)
	uint diffuse;
	int isAABuffer;
} pc;

#endif