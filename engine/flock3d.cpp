#include "engine.h"

#include "core/log.h"
#include "core/random.h"

#include "imgui.h"

#include "input.h"
#include "res.h"
#include "model_loader.h"
#include "terrain.h"

#include "render/imgui_renderer.h"

#include "systems/observer.h"
#include "systems/player.h"
#include "systems/controls.h"
#include "systems/boid.h"

class Flock3DApp : public Engine {
public:
    ENGINE_IMPL(Flock3DApp)

    void init() override;
    void update() override;
    void cleanup() override;

    UniquePtr<Terrain> terrain;
    UniquePtr<BoidSystem> boid_system;
    UniquePtr<TerrainRenderer> terrain_renderer;

    Entity observer, player;
};

void Flock3DApp::init() {
    terrain = UniquePtr(new Terrain());
    srand(time(nullptr));
    terrain->seed = rand();

    BoidConfig boid_cfg;
    boid_system = UniquePtr(new BoidSystem(ecs.get(), thread_pool, boid_cfg));

    terrain_renderer = UniquePtr(new TerrainRenderer(renderer.get(), terrain.get()));
    terrain_renderer->init();

    renderer->add_render_interface(terrain_renderer.get());

    imgui->add_deps({terrain_renderer.get()});

    model_loader->load("models");

    auto& low_poly_bird_model = model_loader->get_model("low_poly_bird");
    auto& mechanical_bird_model = model_loader->get_model("mechanical_bird");
    auto& phoenix_bird_model = model_loader->get_model("phoenix_bird");
    auto& simple_bird_model = model_loader->get_model("simple_bird");
    auto& placeholder_bird_model = model_loader->get_model("placeholder_bird");

    // FlyCamera
    observer = create_observer(ecs.get());
    player = create_player(ecs.get());

    boid_system->set_target(player);

    renderer->set_camera_object(observer);

    auto create_test_entity = [&](const Model& model, glm::vec3 pos) {
        auto entity = ecs->add_entity();
        auto& model_comp = ecs->add_component<Model>(entity);
        model_comp = model;
        auto& transform = ecs->add_component<Transform>(entity);
        transform.reset();
        transform.translation = pos;
        /*
        auto& wireframe = ecs->emplace<WireframeDebugRenderComp>(entity);
        wireframe.color = glm::vec4(1);
        wireframe.width = 2.0f;
        wireframe.blend_factor = 1.0f;
        */
        return entity;
    };

    auto create_boid = [&](glm::vec3 pos, glm::vec3 vel) {
        auto entity = ecs->add_entity();
        auto& model_comp = ecs->add_component<Model>(entity);
        model_comp = placeholder_bird_model;
        auto& transform = ecs->add_component<Transform>(entity);
        transform.reset();
        transform.translation = pos;
        auto& boid = ecs->add_component<Boid>(entity);
        boid.pos = pos;
        boid.vel = vel;
        return entity;
    };

    for (int i = 0; i < 2000; i++) {
        auto pos = random_uniform<glm::vec3>({-1000, 30, -1000}, {1000, 200, 1000});
        auto vel = random_uniform<glm::vec3>({-50, -50, -50}, {50, 50, 50});
        create_boid(pos, vel);
    }

    /*
    create_test_entity(low_poly_bird_model, glm::vec3(-40, 30, 0));
    create_test_entity(low_poly_bird_model, glm::vec3(0, 30, 0));
    create_test_entity(low_poly_bird_model, glm::vec3(40, 30, 0));

    create_test_entity(mechanical_bird_model, glm::vec3(-30, 30, 30));
    create_test_entity(mechanical_bird_model, glm::vec3(30, 30, 30));
    */

    // create_test_entity(phoenix_bird_model, glm::vec3(-30, 30, 60));
    // create_test_entity(phoenix_bird_model, glm::vec3(30, 30, 60));

    // create_test_entity(simple_bird_model, glm::vec3(-60, 30, 30));
    // create_test_entity(simple_bird_model, glm::vec3(60, 30, 30));
}

void Flock3DApp::update() {
    uint32_t pressed_keys = 0;
    if (input->is_key_pressed(SDL_SCANCODE_W)) {
        pressed_keys |= CAM_FORWARD;
    }
    if (input->is_key_pressed(SDL_SCANCODE_A)) {
        pressed_keys |= CAM_LEFT;
    }
    if (input->is_key_pressed(SDL_SCANCODE_S)) {
        pressed_keys |= CAM_BACKWARD;
    }
    if (input->is_key_pressed(SDL_SCANCODE_D)) {
        pressed_keys |= CAM_RIGHT;
    }
    if (input->is_key_pressed(SDL_SCANCODE_Q)) {
        pressed_keys |= CAM_UP;
    }
    if (input->is_key_pressed(SDL_SCANCODE_E)) {
        pressed_keys |= CAM_DOWN;
    }
    static bool is_observing = true;
    static bool is_camera_mouse_enabled = false;
    if (is_observing) {
        if (input->is_mouse_entered(SDL_BUTTON_RIGHT)) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            is_camera_mouse_enabled = true;
        }
        if (input->is_mouse_exited(SDL_BUTTON_RIGHT)) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            is_camera_mouse_enabled = false;
        }
    }
    else {
        is_camera_mouse_enabled = true;
    }

    if (input->is_key_entered(SDL_SCANCODE_P)) {
        is_observing = !is_observing;
        if (is_observing) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            renderer->set_camera_object(observer);
        }
        else {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            renderer->set_camera_object(player);
        }
    }

    glm::ivec2 mouse_offset;
    if (is_camera_mouse_enabled) {
        mouse_offset = input->mouse_movement();
    }
    else {
        mouse_offset = {0, 0};
    }

    float dt = get_cur_deltatime();

    update_observer(ecs.get(), pressed_keys, window_extent, mouse_offset, dt);
    update_player(ecs.get(), *terrain, pressed_keys, window_extent, mouse_offset, dt);

    boid_system->update(dt);

    // _camera->imgui();

    auto res = Res::inst();

    ImGui::Begin("Inspector");

    if (ImGui::CollapsingHeader("Observer")) {
        update_fps_controls_imgui(ecs->get_component<FPSControls>(observer));
    }
    if (ImGui::CollapsingHeader("Player")) {
        update_fps_controls_imgui(ecs->get_component<FPSControls>(player));
    }
    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::DragFloat("scale", &terrain->scale, 0.01f);
        ImGui::SliderInt("octaves", &terrain->octaves, 1, 4);
        ImGui::InputInt("seed", &terrain->seed);
        ImGui::SliderFloat("persistance", &terrain->persistance, 0.0f, 1.0f);
        ImGui::SliderFloat("lacunarity", &terrain->lacunarity, 1.0f, 10.0f);
        ImGui::DragFloat("chunk_width", &terrain->chunk_width, 0.01f);
        ImGui::DragFloat("height_multiplier", &terrain->height_multiplier, 0.01f);
    }
    if (ImGui::CollapsingHeader("Boids")) {
        auto& cfg = boid_system->cfg;
        ImGui::DragFloat("nearby_dist", &cfg.nearby_dist, 0.1f);
        ImGui::DragFloat("avoid_dist", &cfg.avoid_dist, 0.1f);
        ImGui::DragFloat("pos_match_factor", &cfg.pos_match_factor, 0.1f);
        ImGui::DragFloat("vel_match_factor", &cfg.vel_match_factor, 0.1f);
        ImGui::DragFloat("avoid_factor", &cfg.avoid_factor, 0.1f);
        ImGui::DragFloat("target_follow_factor", &cfg.target_follow_factor, 0.1f);
        ImGui::DragFloat("vel_limit", &cfg.vel_limit, 0.1f);
        ImGui::DragFloat("angvel_limit", &cfg.angvel_limit, 0.1f);

        if (ImGui::BeginTable("Boid Table", 3)) {
            ImGui::TableSetupColumn("Boid");
            ImGui::TableSetupColumn("Pos");
            ImGui::TableSetupColumn("Vel");
            ImGui::TableHeadersRow();
            auto boids = ecs->get_component_array<Boid>();
            for (int i = 0; i < boids.ssize(); i++) {
                auto& boid = boids[i];
                std::string boid_label = fmt::format("Boid {}", i);
                std::string boid_index_label = std::to_string(i);
                std::string boid_pos_label = boid_label + "##pos";
                std::string boid_vel_label = boid_label + "##vel";
                ImGui::PushID(boid_label.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", boid_index_label.c_str());
                ImGui::TableNextColumn();
                ImGui::InputFloat3(boid_pos_label.c_str(), (float*)&boid.pos);
                ImGui::TableNextColumn();
                ImGui::InputFloat3(boid_vel_label.c_str(), (float*)&boid.vel);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void Flock3DApp::cleanup() {
    terrain_renderer->cleanup();

    Engine::cleanup();
}

ENGINE_MAIN(Flock3DApp)