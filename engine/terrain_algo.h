#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "terrain.h"

glm::vec3 calc_terrain_with_gradient(glm::vec2 pos, 
    float in_scale, int octaves, uint32_t seed, float persistance, float lacunarity,
    float out_scale_width, float out_scale_height);

inline glm::vec3 calc_terrain_with_gradient(const Terrain& terrain, glm::vec2 pos) {
    return calc_terrain_with_gradient(pos,
        terrain.scale, terrain.octaves, terrain.seed, terrain.persistance, terrain.lacunarity,
        terrain.chunk_width, terrain.height_multiplier);
}
