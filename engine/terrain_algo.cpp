#ifndef GLSL

#define GLM_FORCE_SWIZZLE

#include "terrain_algo.h"

#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <glm/exponential.hpp>

using namespace glm;
using uint = uint32_t;

#endif

float rand(float x) {
    // return float(0.1) * x;
    return fract(sin(x * float(12.9898)) * float(43758.5453));
}

uint pcg_hash(uint value)
{
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// This is taken from here
// https://stackoverflow.com/a/17479300/9778826
float conv_float(uint n)
{
    uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    uint ieeeOne = 0x3F800000u;      // 1.0 in IEEE binary32

    n &= ieeeMantissa;
    n |= ieeeOne;

    float f = uintBitsToFloat(n);
    return f - 1.0;
}

vec2 random_unit_vec2(int x, int y) {
    uint seed = pcg_hash(y ^ pcg_hash(x));
    uint s1 = seed ^ pcg_hash(seed);
    uint s2 = s1 ^ pcg_hash(s1);
    float f1 = (conv_float(s1) - float(0.5)) * float(2.0);
    float f2 = (conv_float(s2) - float(0.5)) * float(2.0);
    return vec2(f1, f2);
}

// From https://github.com/BrianSharpe/Wombat/blob/master/Perlin2D_Deriv.glsl
vec3 perlin2d_with_deriv(vec2 P)
{
    // establish our grid cell and unit position
    vec2 Pi_f = floor(P);
    vec4 Pf_Pfmin1 = P.xyxy - vec4( Pi_f, Pi_f + vec2(1.0) );

    // calculate the hash
    ivec2 Pi = ivec2(Pi_f);
    ivec4 Pt = ivec4( Pi.x, Pi.y, Pi.x + 1, Pi.y + 1 );

    uint seed_tmp1 = pcg_hash(Pi.x);
    uint seed_tmp2 = pcg_hash(Pi.x + 1);
    uint seed1 = pcg_hash(Pi.y ^ seed_tmp1);
    uint seed2 = pcg_hash(Pi.y ^ seed_tmp2);
    uint seed3 = pcg_hash((Pi.y + 1) ^ seed_tmp1);
    uint seed4 = pcg_hash((Pi.y + 1) ^ seed_tmp2);
    uint s1x = seed1 ^ pcg_hash(seed1);
    uint s2x = seed2 ^ pcg_hash(seed2);
    uint s3x = seed3 ^ pcg_hash(seed3);
    uint s4x = seed4 ^ pcg_hash(seed4);
    uint s1y = s1x ^ pcg_hash(s1x);
    uint s2y = s2x ^ pcg_hash(s2x);
    uint s3y = s3x ^ pcg_hash(s3x);
    uint s4y = s4x ^ pcg_hash(s4x);
    vec4 grad_x = vec4(conv_float(s1x), conv_float(s2x), conv_float(s3x), conv_float(s4x)) - float(0.5);
    vec4 grad_y = vec4(conv_float(s1y), conv_float(s2y), conv_float(s3y), conv_float(s4y)) - float(0.5);
    vec4 norm = inversesqrt( grad_x * grad_x + grad_y * grad_y );
    grad_x *= norm;
    grad_y *= norm;
    vec4 dotval = ( grad_x * Pf_Pfmin1.xzxz + grad_y * Pf_Pfmin1.yyww );

    //  C2 Interpolation
    vec4 blend = Pf_Pfmin1.xyxy * Pf_Pfmin1.xyxy * ( Pf_Pfmin1.xyxy * ( Pf_Pfmin1.xyxy * ( Pf_Pfmin1.xyxy * vec2( 6.0, 0.0 ).xxyy + vec2( -15.0, 30.0 ).xxyy ) + vec2( 10.0, -60.0 ).xxyy ) + vec2( 0.0, 30.0 ).xxyy );

    //  Convert our data to a more parallel format
    vec3 dotval0_grad0 = vec3( dotval.x, grad_x.x, grad_y.x );
    vec3 dotval1_grad1 = vec3( dotval.y, grad_x.y, grad_y.y );
    vec3 dotval2_grad2 = vec3( dotval.z, grad_x.z, grad_y.z );
    vec3 dotval3_grad3 = vec3( dotval.w, grad_x.w, grad_y.w );

    //  evaluate common constants
    vec3 k0_gk0 = dotval1_grad1 - dotval0_grad0;
    vec3 k1_gk1 = dotval2_grad2 - dotval0_grad0;
    vec3 k2_gk2 = dotval3_grad3 - dotval2_grad2 - k0_gk0;

    //  calculate final noise + deriv
    vec3 results = dotval0_grad0
                    + blend.x * k0_gk0
                    + blend.y * ( k1_gk1 + blend.x * k2_gk2 );
    results.yz += blend.zw * ( vec2( k0_gk0.x, k1_gk1.x ) + blend.yx * k2_gk2.xx );
    return results * float(1.4142135623730950488016887242097);  // scale things to a strict -1.0->1.0 range  *= 1.0/sqrt(0.5)
}

vec3 calc_terrain_with_gradient(vec2 pos, 
    float in_scale, int octaves, uint seed, float persistance, float lacunarity,
    float out_scale_width, float out_scale_height) {

    vec2 in_uv = pos / out_scale_width;

    float amplitude = 1.0;
    float frequency = float(1.0) / in_scale;
    float noise_height = 0;
    vec2 grad = vec2(0);
    for (int k = 0; k < octaves; k++) {
        // vec2 rand_offset = vec2(rand(seed + 2*k), rand(seed + 2*k + 1));
        // vec2 perlin_in = frequency * in_uv + rand_offset;
        vec2 perlin_in = frequency * in_uv;
        vec3 perlin_out = amplitude * perlin2d_with_deriv(perlin_in);
        noise_height += perlin_out.x;
        grad += frequency * perlin_out.yz;
        amplitude *= persistance;
        frequency *= lacunarity;
    }
    noise_height *= out_scale_height;
    grad *= (out_scale_height / out_scale_width);
    return vec3(noise_height, grad.x, grad.y);
}
