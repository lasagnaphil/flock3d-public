#include "input.h"

#include <imgui.h>
#include <imgui_internal.h>

void Input::before_update() {
    // update keyboard state
    memcpy(_prev_keys, _curr_keys, SDL_NUM_SCANCODES * sizeof(Uint8));
    memcpy(_curr_keys, SDL_GetKeyboardState(NULL), SDL_NUM_SCANCODES * sizeof(Uint8));

    // update mouse state
    _prev_mouse = _curr_mouse;
    _prev_mouse_pos = _curr_mouse_pos;
    _curr_mouse = SDL_GetMouseState(&_curr_mouse_pos.x, &_curr_mouse_pos.y);
}

void Input::respond_to_event(SDL_Event* e) {
    switch (e->type) {
        case SDL_MOUSEMOTION: {
            _mouse_movement.x += e->motion.xrel;
            _mouse_movement.y += e->motion.yrel;
        } break;
    }
}

void Input::after_update() {
    // reset mouse movement
    _mouse_movement = {0, 0};
}

bool Input::is_key_pressed(SDL_Scancode key){
    auto& io = ImGui::GetIO();
    return _curr_keys[key] && !io.WantCaptureKeyboard;
}

bool Input::is_key_entered(SDL_Scancode key) {
    auto& io = ImGui::GetIO();
    return _curr_keys[key] && !_prev_keys[key] && !io.WantCaptureKeyboard;
}

bool Input::is_key_exited(SDL_Scancode key) {
    auto& io = ImGui::GetIO();
    return !_curr_keys[key] && _prev_keys[key] && !io.WantCaptureKeyboard;
}

bool Input::is_mouse_pressed(Uint8 button) {
    auto& io = ImGui::GetIO();
    return (_curr_mouse & SDL_BUTTON(button)) != 0 && !io.WantCaptureMouse;
}

bool Input::is_mouse_entered(Uint8 button) {
    auto& io = ImGui::GetIO();
    return (_curr_mouse & SDL_BUTTON(button)) != 0 &&
           (_prev_mouse & SDL_BUTTON(button)) == 0 &&
           !io.WantCaptureMouse;
}

bool Input::is_mouse_exited(Uint8 button) {
    auto& io = ImGui::GetIO();
    return (_curr_mouse & SDL_BUTTON(button)) == 0 &&
           (_prev_mouse & SDL_BUTTON(button)) != 0 &&
           !io.WantCaptureMouse;
}

bool Input::is_mouse_on_imgui() {
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g->HoveredWindow != NULL) return true;
    return false;
}

glm::ivec2 Input::mouse_pos() {
    return _curr_mouse_pos;
}

glm::ivec2 Input::mouse_movement() {
    // return _curr_mouse_pos - _prev_mouse_pos;
    return _mouse_movement;
}
