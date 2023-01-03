#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

#include "ecs.h"

struct Terrain;

Entity create_player(ECS* ecs);

void update_player(ECS* ecs, const Terrain& terrain, 
    uint32_t pressed_keys, glm::ivec2 screen_extent, glm::ivec2 mouse_offset, float dt);

