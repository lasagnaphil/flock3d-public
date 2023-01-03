#pragma once

#include "render/renderer.h"


class MeshRenderer : public RenderInterface {
public:
	MeshRenderer(Renderer* renderer, ECS* ecs) : RenderInterface(renderer), _ecs(ecs) {}

	void init();
	void render(VkCommandBuffer command_buffer) override;
	void cleanup();

private:
	void create_graphics_pipeline();

	void create_descriptor_set_layout();
	void create_descrtptor_sets();

    VkPipelineLayout _graphics_pipeline_layout;
    VkPipeline _graphics_pipeline;

    VkDescriptorSetLayout _material_descriptor_set_layout;
    DescriptorSet _material_descriptor_set;

    ECS* _ecs;
};
