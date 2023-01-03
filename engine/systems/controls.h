#pragma once

#include "components.h"

void update_fps_controls_direction(FPSControls& controls, glm::ivec2 mouse_offset);
void update_fps_controls_camera(FPSControls& controls, Camera& camera, glm::vec3 pos, glm::ivec2 screen_extent);
void update_fps_controls_imgui(FPSControls& controls);
