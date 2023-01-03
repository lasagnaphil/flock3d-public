#pragma once

#include <glm/ext.hpp>
#include <SDL_events.h>

class Input {
public:
    void before_update();
    void respond_to_event(SDL_Event* e);
    void after_update();

    bool is_key_pressed(SDL_Scancode key);
    bool is_key_entered(SDL_Scancode key);
    bool is_key_exited(SDL_Scancode key);

    bool is_mouse_pressed(Uint8 button);
    bool is_mouse_entered(Uint8 button);
    bool is_mouse_exited(Uint8 button);

    bool is_mouse_on_imgui();

    glm::ivec2 mouse_pos();
    glm::ivec2 mouse_movement();

private:
    Uint8 _curr_keys[SDL_NUM_SCANCODES] = {};
    Uint8 _prev_keys[SDL_NUM_SCANCODES] = {};

    Uint32 _curr_mouse = {};
    Uint32 _prev_mouse = {};
    glm::ivec2 _curr_mouse_pos = {};
    glm::ivec2 _prev_mouse_pos = {};

    glm::ivec2 _mouse_movement;
};