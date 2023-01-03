#include "engine.h"
#include "SDL_hints.h"
#include "SDL_video.h"

#include <SDL.h>
#include <physfs.h>
#include <imgui.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <imgui_impl_sdl.h>

#include "nanothread/nanothread.h"

#include "core/log.h"
#include "core/win32_utils.h"

#include "ecs.h"
#include "res.h"
#include "input.h"
#include "mesh.h"
#include "model_loader.h"

#include "render/renderer.h"
#include "render/im3d_renderer.h"
#include "render/imgui_renderer.h"
#include "render/wireframe_renderer.h"
#include "render/mesh_renderer.h"
#include "render/linavg_renderer.h"

#define TRACY_ENABLE
#include "tracy/Tracy.hpp"

Engine* Engine::_inst = nullptr;

void Engine::check_instance() {
    if (_inst) {
        log_error("An instance of the engine has already been created!");
        std::abort();
    }
}

Engine::Engine(int argc, char** argv) {
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = std::string(argv[i]);
    }
}

Engine::~Engine() {
}

Engine* Engine::create(int argc, char** argv) {
    check_instance();
    _inst = new Engine(argc, argv);
    return _inst;
}

Engine* Engine::instance() {
    if (_inst) return _inst;
    else {
        log_error("Engine instance not created yet!");
        std::abort();
    }
}

void Engine::init_internal() {
    ZoneScoped; 

    log_init("log.txt");
    log_set_minimum_level(LOG_INFO);

    thread_pool = pool_create(std::thread::hardware_concurrency() - 2);

    PHYSFS_init(args[0].c_str());
    const char* search_paths[] = {
            "assets", "assets.zip",
    };
    for (const char* search_path : search_paths) {
        if (PHYSFS_mount(search_path, "/", false)) break;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // TODO: investigate why this new flag in SDL 2.0.22 isn't working
    if (!SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "1", SDL_HINT_OVERRIDE)) {
        log_warn("Warning: Mouse relative mode center not enabled!");
    }

    SDL_Init(SDL_INIT_VIDEO);

    int display_count, display_index = 0, mode_index = 0;
    SDL_DisplayMode mode;
    if ((display_count = SDL_GetNumVideoDisplays()) < 1) {
        log_error("SDL_GetNumVideoDisplays returned: {}", display_count);
    }
    if (SDL_GetDisplayMode(display_index, mode_index, &mode) != 0) {
        log_error("SDL_GetDisplayMode failed: {}", SDL_GetError());
    }

    log_info("SDL_GetDisplayMode(0, 0, &mode):\t\t{} bpp\t{} x {}",
        SDL_BITSPERPIXEL(mode.format), mode.w, mode.h);

    window_extent.x = mode.w;
    window_extent.y = mode.h;
    
    window = SDL_CreateWindow(
            "flock3d",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            window_extent.x,
            window_extent.y,
            SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);

    // win32_open_console();

    Res::initialize();

    ecs = UniquePtr(new ECS);

    input = UniquePtr(new Input);
    renderer = UniquePtr(new Renderer(window, ecs.get()));
    model_loader = UniquePtr(new ModelLoader(renderer.get()));

    renderer->init();

    im3d = UniquePtr(new Im3dRenderer(renderer.get()));
    wireframe = UniquePtr(new WireframeRenderer(renderer.get()));
    mesh_renderer = UniquePtr(new MeshRenderer(renderer.get(), ecs.get()));
    linavg_renderer = UniquePtr(new LinaVGRenderer(renderer.get()));

    imgui = UniquePtr(new ImGuiRenderer(renderer.get()));
    imgui->set_deps({im3d.get(), wireframe.get(), mesh_renderer.get(), linavg_renderer.get()});

    im3d->init();
    imgui->init();
    wireframe->init();
    mesh_renderer->init();
    linavg_renderer->init();

    renderer->add_render_interface(im3d.get());
    renderer->add_render_interface(wireframe.get());
    renderer->add_render_interface(mesh_renderer.get());
    renderer->add_render_interface(linavg_renderer.get());
    renderer->add_render_interface(imgui.get());

    ImGui_ImplSDL2_InitForVulkan(window);
}

void Engine::cleanup_internal() {
    ZoneScoped; 

    mesh_renderer->cleanup();
    wireframe->cleanup();
    imgui->cleanup();
    im3d->cleanup();

    renderer->cleanup();

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    Res::cleanup();

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::run() {
    init_internal();

    init();

    is_initialized = true;

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            switch (e.type) {
                case SDL_QUIT: {
                    quit = true;
                } break;
                case SDL_WINDOWEVENT: {
                    renderer->respond_to_window_event(&e);
                } break;
                default: {
                    input->respond_to_event(&e);
                }
            }
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_WINDOWEVENT) {
                renderer->respond_to_window_event(&e);
            }
            else {
                input->respond_to_event(&e);
            }
        }

        input->before_update();

        static uint64_t start_time_counter = SDL_GetPerformanceCounter();
        static uint64_t prev_time_counter = SDL_GetPerformanceCounter();

        im3d->new_frame();
        imgui->new_frame();

        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        uint64_t cur_time_counter = SDL_GetPerformanceCounter();
        uint64_t perf_freq = SDL_GetPerformanceFrequency();
        _cur_time = (float)(cur_time_counter - start_time_counter) / (float)perf_freq;
        _cur_deltatime = (float)(cur_time_counter - prev_time_counter) / (float)perf_freq;

        prev_time_counter = SDL_GetPerformanceCounter();

        static bool is_full_screen = false;
        static glm::ivec2 orig_window_extent; 
        if (input->is_key_entered(SDL_SCANCODE_F11)) {
            is_full_screen = !is_full_screen;
            if (is_full_screen) {
                orig_window_extent = renderer->get_window_extent();

                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                int display_count = 0, display_index = 0, mode_index = 0;
                SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

                if ((display_count = SDL_GetNumVideoDisplays()) < 1) {
                    log_error("SDL_GetNumVideoDisplays returned: {}", display_count);
                }
                if (SDL_GetDisplayMode(display_index, mode_index, &mode) != 0) {
                    log_error("SDL_GetDisplayMode failed: {}", SDL_GetError());
                }

                log_info("SDL_GetDisplayMode(0, 0, &mode):\t\t{} bpp\t{} x {}",
                    SDL_BITSPERPIXEL(mode.format), mode.w, mode.h);

                if (SDL_SetWindowDisplayMode(window, &mode) != 0) {
                    log_error("SDL_SetWindowDisplayMode failed: {}", SDL_GetError());
                }
            }
            else {
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, orig_window_extent.x, orig_window_extent.y);
            }
        }

        update();

        renderer->render_imgui();

        ImGui::Render();

        renderer->begin_frame();
        renderer->render();
        renderer->end_frame();

        input->after_update();
    }

    renderer->wait_until_device_idle();

    cleanup();

    cleanup_internal();
}

