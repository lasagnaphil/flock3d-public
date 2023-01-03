#pragma once

#include <SDL.h>

#include <string>
#include <glm/vec2.hpp>

#include "core/vector.h"
#include "core/unique_ptr.h"
#include "ecs.h"

struct Pool;

class Renderer;
class Input;
class ModelLoader;
class Camera;
class Terrain;
class BoidSystem;

class Im3dRenderer;
class ImGuiRenderer;
class WireframeRenderer;
class MeshRenderer;
class TerrainRenderer;
class LinaVGRenderer;

class Engine {
protected:
    static void check_instance();
    Engine(int argc, char** argv);
    virtual ~Engine();
    static Engine* _inst;

public:
    static Engine* create(int argc, char** argv);
    static Engine* instance();

    virtual void init() {}
    virtual void cleanup() {}
    virtual void update() {}

    void run();

    float get_cur_time() { return _cur_time; }
    float get_cur_deltatime() { return _cur_deltatime; }

    Vector<std::string> args;
    struct SDL_Window* window = nullptr;

    bool is_initialized = false;
    int frame_number = 0;
    glm::ivec2 window_extent;

    Pool* thread_pool = nullptr;

    UniquePtr<ECS> ecs;

    UniquePtr<Renderer> renderer;
    UniquePtr<Input> input;
    UniquePtr<ModelLoader> model_loader;

    UniquePtr<Im3dRenderer> im3d;
    UniquePtr<ImGuiRenderer> imgui;
    UniquePtr<WireframeRenderer> wireframe;
    UniquePtr<MeshRenderer> mesh_renderer;
    UniquePtr<LinaVGRenderer> linavg_renderer;

private:
    float _cur_time;
    float _cur_deltatime;

    void init_internal();
    void cleanup_internal();
};

#define ENGINE_IMPL(AppName) \
static AppName* create(int argc, char* argv[]) { \
    check_instance(); \
    auto app = new AppName(argc, argv); \
    _inst = app; \
    return app; \
} \
AppName(int argc, char* argv[]) : Engine(argc, argv) {} \

#define ENGINE_MAIN(AppName) \
int main(int argc, char* argv[]) { \
    auto engine = AppName::create(argc, argv); \
    engine->run(); \
    return 0; \
}


