#include "engine.h"

#include "core/log.h"
#include "core/random.h"

#include "imgui.h"

#include "res.h"

#include "linavg_demo_screens.hpp"
#include "render/renderer.h"

#include "LinaVG.hpp"

class LinaVGTestApp : public Engine {
public:
	ENGINE_IMPL(LinaVGTestApp)

	LinaVG::Examples::DemoScreens demo_screens;

	void init() override;
	void update() override;
	void cleanup() override;
};

void LinaVGTestApp::init() {
	auto screen_extent = renderer->get_window_extent();
	constexpr float fov = 100.0f;
	Entity it_camera = ecs->add_entity();
	auto& camera = ecs->add_component<Camera>(it_camera);
	camera.position = glm::vec3(0, 0, 0);
	camera.rotation = glm::mat3(1);
    camera.proj_mat = glm::ortho(0.0f, (float)screen_extent.x, (float)screen_extent.y, 0.0f, -1.0f, 1.0f);
	renderer->set_camera_object(it_camera);

	demo_screens.Initialize();
	demo_screens.m_FrameTime = 0.0f;
	demo_screens.m_FPS = 0;
	demo_screens.m_CurrentScreen = 1;

	auto res = Res::inst();
}

void LinaVGTestApp::update() {
	// Setup style, give a gradient color from red to blue.
	LinaVG::StyleOptions style;
	style.isFilled      = true;
	style.color.start = LinaVG::Vec4(1, 0, 0, 1);
	style.color.end   = LinaVG::Vec4(0, 0, 1, 1);

	// Draw a 200x200 rectangle starting from 300, 300.
	const auto min = LinaVG::Vec2(300, 300);
	const auto max = LinaVG::Vec2(500, 500);
	LinaVG::DrawRect(min, max, style);

	// demo_screens.ShowDemoScreen1_Shapes();
	// demo_screens.ShowDemoScreen2_Colors();
	// demo_screens.ShowDemoScreen3_Outlines();
	// demo_screens.ShowDemoScreen4_Lines();

	// demo_screens.PreEndFrame();
}

void LinaVGTestApp::cleanup() {

}

ENGINE_MAIN(LinaVGTestApp)