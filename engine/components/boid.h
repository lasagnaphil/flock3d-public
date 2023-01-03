#pragma once

#include <glm/vec3.hpp>
#include "core/vector.h"

struct Boid {
    glm::vec3 pos;
    glm::vec3 vel;

    Vector<int32_t> nearby_boids;
    Vector<int32_t> avoid_boids;
    int32_t cell_id;
};

