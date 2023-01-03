#pragma once

#include "renderer.h"
#include "LinaVG.hpp"

struct LinaVGPushConstants {
    // linavg_textured.frag (32 bytes)
	glm::vec2 tiling;
	glm::vec2 offset;
	glm::vec4 tint;

	// linavg_sdf_text.frag (48 bytes)
    float softness; 
    float thickness; 
    int outlineEnabled; 
    int useOutlineOffset; 
    glm::vec2 outlineOffset; 
    float outlineThickness; 
    int flipAlpha;
    glm::vec4 outlineColor; 

    // linavg_rounded_gradient.frag (40 bytes)
	glm::vec4 startColor;
	glm::vec4 endColor;
	int  gradientType;
	float radialSize;

    // shared (8 bytes)
	uint32_t diffuse;
	int32_t isAABuffer;
};

class LinaVGRenderer : public RenderInterface {
public:
	enum class RenderType {
		Default, RoundedGradient, Textured, SimpleText, SDFText, _RenderTypeCount
	};
	static constexpr int RenderTypeCount = (int) RenderType::_RenderTypeCount;

	LinaVGRenderer(Renderer* renderer) : RenderInterface(renderer) {}

	void init();
	void begin_frame() override;
	void end_frame() override;
	void render(VkCommandBuffer command_buffer) override;
	void cleanup();

	void render_default(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf);
	void render_gradient(VkCommandBuffer command_buffer, LinaVG::GradientDrawBuffer& buf);
	void render_texture(VkCommandBuffer command_buffer, LinaVG::TextureDrawBuffer& buf);
	void render_simple_text(VkCommandBuffer command_buffer, LinaVG::SimpleTextDrawBuffer& buf);
	void render_sdf_text(VkCommandBuffer command_buffer, LinaVG::SDFTextDrawBuffer& buf);

private:

	bool _is_initialized = false;

	VkPipelineLayout _pipeline_layout;
	Array<VkPipeline, RenderTypeCount> _pipelines;

	Array<DynamicBuffer, RenderTypeCount> _vertex_buffers;
	Array<DynamicBuffer, RenderTypeCount> _index_buffers;

	int _debug_current_draw_calls = 0;
	int _debug_current_triangle_count = 0;
	int _debug_current_vertex_count = 0;

	void exec_render_pass(VkCommandBuffer command_buffer, int drawOrder, LinaVG::DrawBufferShapeType shapeType);

	void copy_buffer_data_to_gpu(LinaVG::DrawBuffer& buf, RenderType render_type);
	void cmd_bind_buffers(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf, RenderType render_type);
	void cmd_set_scissors(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf);
	void cmd_draw_indexed(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf);
};