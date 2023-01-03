#pragma once

#include "renderer.h"

class ImDrawCmd;

struct ImGuiPushConstants {
    glm::vec2 scale;
    glm::vec2 translate;
    uint32_t tex_id;
};

class ImGuiRenderer : public RenderInterface {
public:
    ImGuiRenderer(Renderer* renderer) : RenderInterface(renderer) {}

    void init();
    void new_frame();
    void render(VkCommandBuffer command_buffer) override;
    void cleanup();

    void setup_render_state(VkCommandBuffer command_buffer);

    DynamicBuffer _vertex_buffer;
    DynamicBuffer _index_buffer;

    bool _is_initialized = false;
    VkPipelineLayout _pipeline_layout = {};
    VkPipeline _graphics_pipeline = {};

    VkSampler _font_sampler = {};
};
