#include "controls.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>

#include "imgui.h"

void update_fps_controls_direction(FPSControls& controls, glm::ivec2 mouse_offset) {
    controls.yaw += mouse_offset.x * controls.mouse_sensitivity;
    controls.pitch -= mouse_offset.y * controls.mouse_sensitivity;
    if (controls.constrain_pitch) {
        controls.pitch = glm::clamp(controls.pitch, -89.0f, 89.0f);
    }

    controls.front.x = glm::cos(glm::radians(controls.yaw)) * glm::cos(glm::radians(controls.pitch));
    controls.front.y = glm::sin(glm::radians(controls.pitch));
    controls.front.z = glm::sin(glm::radians(controls.yaw)) * glm::cos(glm::radians(controls.pitch));
    controls.front = glm::normalize(controls.front);
    controls.right = glm::normalize(glm::cross(controls.front, glm::vec3(0, 1, 0)));
    controls.up = glm::normalize(glm::cross(controls.right, controls.front));
}

void update_fps_controls_camera(FPSControls& controls, Camera& camera, glm::vec3 pos, glm::ivec2 screen_extent) {
    camera.rotation = glm::mat3(controls.right, controls.up, controls.front);
    camera.position = pos;
    camera.proj_mat = glm::perspectiveFov(glm::radians(controls.fov), 
        (float)screen_extent.x, (float)screen_extent.y, 
        controls.near, controls.far);
    camera.proj_mat[1][1] *= -1;
}

void update_fps_controls_imgui(FPSControls& controls) {
    ImGui::DragFloat("yaw", &controls.yaw, 0.01f);
    ImGui::DragFloat("pitch", &controls.pitch, 0.01f);
    ImGui::DragFloat("fov", &controls.fov, 0.01f);
    ImGui::DragFloat("near", &controls.near, 0.01f);
    ImGui::DragFloat("far", &controls.far, 0.01f);
    ImGui::DragFloat("mouse_sensitivity", &controls.mouse_sensitivity, 0.01f);
}

