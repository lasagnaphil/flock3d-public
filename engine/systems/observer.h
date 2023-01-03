#pragma once

#include "ecs.h"

Entity create_observer(ECS* ecs);

void update_observer(ECS* ecs, 
	uint32_t pressed_keys, glm::ivec2 screen_extent, glm::ivec2 mouse_offset, float dt);

