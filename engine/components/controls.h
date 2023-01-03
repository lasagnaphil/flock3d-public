#pragma once

#include <glm/vec3.hpp>

struct FPSControls {
    glm::vec3 front, up, right;
    float yaw = 0.0f, pitch = 0.0f;
    float fov = 90.0f;
    float near = 0.001f, far = 1000.0f;
    float mouse_sensitivity = 0.1f;
    bool constrain_pitch = true;
};

struct Observer {
    glm::vec3 pos = {0, 0, 0};

    float movement_speed = 20.0f;
};

