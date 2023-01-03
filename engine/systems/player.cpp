#include "player.h"

#include "observer.h"

#include "terrain.h"
#include "terrain_algo.h"

#include "ecs.h"

#include "systems/controls.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>

Entity create_player(ECS* ecs) {
    Entity it = ecs->add_entity();
    ecs->add_component<Player>(it);
    ecs->add_component<FPSControls>(it);
    ecs->add_component<Camera>(it);
    return it;
}

void update_player(ECS* ecs, const Terrain& terrain, 
    uint32_t pressed_keys, glm::ivec2 screen_extent, glm::ivec2 mouse_offset, float dt) {
    ecs->query<Player, FPSControls, Camera>().foreach([&](Entity entity, Player& player, FPSControls& controls, Camera& camera) {
        update_fps_controls_direction(controls, mouse_offset);

        glm::vec2 plane_front = glm::normalize(glm::vec2(controls.front.x, controls.front.z));
        glm::vec2 plane_right = glm::vec2(-plane_front.y, plane_front.x);
        glm::vec2 dir = glm::vec2(0);
        if (pressed_keys & CAM_FORWARD) {
            dir += plane_front;
        }
        if (pressed_keys & CAM_BACKWARD) {
            dir -= plane_front;
        }
        if (pressed_keys & CAM_RIGHT) {
            dir += plane_right;
        }
        if (pressed_keys & CAM_LEFT) {
            dir -= plane_right;
        }
        float dir_l = glm::length(dir);
        if (dir_l > 1e-6f) {
            dir /= dir_l;
        }
        glm::vec2 plane_offset = (player.movement_speed * dt) * dir;

        // TODO: Calculate actual next position using terrain
        glm::vec2 player_pos_plane = glm::vec2(player.pos.x, player.pos.z);
        glm::vec3 res = calc_terrain_with_gradient(terrain, player_pos_plane);
        glm::vec2 cur_grad = glm::vec2(res.y, res.z);
        float cos_slope = glm::inversesqrt(1.0f + cur_grad.x * cur_grad.x + cur_grad.y * cur_grad.y);

        glm::vec2 new_pos = player_pos_plane + plane_offset * cos_slope;

        res = calc_terrain_with_gradient(terrain, new_pos);
        player.pos.x = new_pos.x;
        player.pos.y = player.camera_height + res.x;
        player.pos.z = new_pos.y;

        update_fps_controls_camera(controls, camera, player.pos, screen_extent);
    });
}

