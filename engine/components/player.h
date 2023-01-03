#pragma once

#include <glm/vec3.hpp>

struct Player {
    glm::vec3 pos;
    glm::vec3 vel;

    float capsule_radius = 0.25f;
    float capsule_height = 1.0f;
    float camera_height = 0.75f;

    float movement_speed = 10.0f;
};

