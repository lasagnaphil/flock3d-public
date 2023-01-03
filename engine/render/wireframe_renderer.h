#pragma once

#include "renderer.h"

class WireframeRenderer : public RenderInterface {
public:
    WireframeRenderer(Renderer* renderer) : RenderInterface(renderer) {}

    void init();
    void render(VkCommandBuffer command_buffer) override;
    void cleanup();

private:
    void create_graphics_pipelines();

    VkPipelineLayout _pipeline_layout;
    VkPipeline _pipeline;
};