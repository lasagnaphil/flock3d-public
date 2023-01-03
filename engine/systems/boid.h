#pragma once

#include "ecs.h"
#include "core/vector.h"
#include "core/map.h"

#include <parallel_hashmap/phmap.h>

#include <glm/vec3.hpp>

struct Pool;

struct BoidConfig {
	float nearby_dist = 25.0f;
	float avoid_dist = 5.0f;
	float pos_match_factor = 1.0f;
	float vel_match_factor = 0.1f;
	float avoid_factor = 5.0f;
	float target_follow_factor = 1.0f;
	float vel_limit = 5.0f;
	float angvel_limit = 2.0f;

	float cell_size = 10.0f;
};

struct BoidCell {
	Vector<int32_t> boid_indices;
};

struct BoidCellHash {
	size_t operator()(glm::ivec3 coord) const {
		return (73856093 * coord.x) ^ (19349663 * coord.y) ^ (83492791 * coord.z);
	}
};

class BoidSystem {
public:
	BoidSystem(ECS* ecs, Pool* thread_pool, BoidConfig cfg);

	ECS* ecs;
	Pool* thread_pool;
	BoidConfig cfg;
	ParallelMap<glm::ivec3, BoidCell, BoidCellHash> cell_map;

	Entity target;

	void update(float dt);

	void set_target(Entity target) { this->target = target; }
};