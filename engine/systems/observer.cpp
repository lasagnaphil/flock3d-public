#include "systems/observer.h"
#include "systems/controls.h"

#include "ecs.h"

Entity create_observer(ECS* ecs) {
    Entity it = ecs->add_entity();
    ecs->add_component<Observer>(it);
    ecs->add_component<FPSControls>(it);
    ecs->add_component<Camera>(it);
    return it;
}

void update_observer(ECS* ecs, uint32_t pressed_keys, glm::ivec2 screen_extent, glm::ivec2 mouse_offset, float dt) {
    ecs->query<Observer, FPSControls, Camera>().foreach(
        [&](Entity entity, Observer& observer, FPSControls& controls, Camera& camera) {
        update_fps_controls_direction(controls, mouse_offset);

        float dx = observer.movement_speed * dt;
        if (pressed_keys & CAM_FORWARD) {
            observer.pos += controls.front * dx;
        }
        if (pressed_keys & CAM_BACKWARD) {
            observer.pos -= controls.front * dx;
        }
        if (pressed_keys & CAM_RIGHT) {
            observer.pos += controls.right * dx;
        }
        if (pressed_keys & CAM_LEFT) {
            observer.pos -= controls.right * dx;
        }
        if (pressed_keys & CAM_UP) {
            observer.pos += controls.up * dx;
        }
        if (pressed_keys & CAM_DOWN) {
            observer.pos -= controls.up * dx;
        }

        update_fps_controls_camera(controls, camera, observer.pos, screen_extent);
    });
}

