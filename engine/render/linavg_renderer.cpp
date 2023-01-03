#include "linavg_renderer.h"

#include "render/renderer.h"
#include "res.h"
#include "vk_utils.h"
#include "vulkan/vulkan_core.h"

#include "LinaVG.hpp"
#include "Core/Backend.hpp"

#define TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "Core/Renderer.hpp"

void LinaVGRenderer::init() {
	auto extent = _renderer->get_window_extent();

	LinaVG::Config.displayPosX = 0;
	LinaVG::Config.displayPosY = 0;
	LinaVG::Config.displayWidth = extent.x;
	LinaVG::Config.displayHeight = extent.y;

	LinaVG::Internal::g_rendererData.m_defaultBuffers.reserve(LinaVG::Config.defaultBufferReserve);
	LinaVG::Internal::g_rendererData.m_gradientBuffers.reserve(LinaVG::Config.gradientBufferReserve);
	LinaVG::Internal::g_rendererData.m_textureBuffers.reserve(LinaVG::Config.textureBufferReserve);
	LinaVG::Internal::g_rendererData.m_simpleTextBuffers.reserve(LinaVG::Config.textBuffersReserve);
	LinaVG::Internal::g_rendererData.m_sdfTextBuffers.reserve(LinaVG::Config.textBuffersReserve);

	if (LinaVG::Config.textCachingEnabled)
		LinaVG::Internal::g_rendererData.m_textCache.reserve(LinaVG::Config.textCacheReserve);

	if (LinaVG::Config.textCachingSDFEnabled)
		LinaVG::Internal::g_rendererData.m_sdfTextCache.reserve(LinaVG::Config.textCacheSDFReserve);

	Shader shader_vs = _renderer->load_shader_from_file("shaders/linavg_default.vert.spv", ShaderType::Vertex);
	Shader shader_fs_default = _renderer->load_shader_from_file("shaders/linavg_default.frag.spv", ShaderType::Fragment);
	Shader shader_fs_rounded_gradient = _renderer->load_shader_from_file("shaders/linavg_rounded_gradient.frag.spv", ShaderType::Fragment);
	Shader shader_fs_textured = _renderer->load_shader_from_file("shaders/linavg_textured.frag.spv", ShaderType::Fragment);
	Shader shader_fs_simple_text = _renderer->load_shader_from_file("shaders/linavg_simple_text.frag.spv", ShaderType::Fragment);
	Shader shader_fs_sdf_text = _renderer->load_shader_from_file("shaders/linavg_sdf_text.frag.spv", ShaderType::Fragment);

	Array<Array<VkPipelineShaderStageCreateInfo, 2>, RenderTypeCount> shader_stages_per_type;
	shader_stages_per_type[(int)RenderType::Default] = {shader_vs.stage_info(), shader_fs_default.stage_info()};
	shader_stages_per_type[(int)RenderType::RoundedGradient] = {shader_vs.stage_info(), shader_fs_rounded_gradient.stage_info()};
	shader_stages_per_type[(int)RenderType::Textured] = {shader_vs.stage_info(), shader_fs_textured.stage_info()};
	shader_stages_per_type[(int)RenderType::SimpleText] = {shader_vs.stage_info(), shader_fs_simple_text.stage_info()};
	shader_stages_per_type[(int)RenderType::SDFText] = {shader_vs.stage_info(), shader_fs_sdf_text.stage_info()};

	Array<VkVertexInputBindingDescription, 1> binding_descs = {
		VkVertexInputBindingDescription {
			.stride = sizeof(LinaVG::Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		}
	};

	Array<VkVertexInputAttributeDescription, 3> attribute_descs = {
		VkVertexInputAttributeDescription {
			.location = 0,
			.binding = binding_descs[0].binding,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(LinaVG::Vertex, pos)
		},
		VkVertexInputAttributeDescription {
			.location = 1,
			.binding = binding_descs[0].binding,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(LinaVG::Vertex, uv)
		},
		VkVertexInputAttributeDescription {
			.location = 2,
			.binding = binding_descs[0].binding,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = offsetof(LinaVG::Vertex, col)
		}
	};

	auto vertex_input_info = vkuCreatePipelineVertexInputStateCreateInfo(binding_descs, attribute_descs);

    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = _renderer->default_viewport();
    VkRect2D scissor = _renderer->default_scissor();

    auto viewport_state = vkuCreatePipelineViewportStateCreateInfo(&viewport, &scissor);

    VkPipelineRasterizationStateCreateInfo raster_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f
    };

    // TODO: Change this if we want multisampling
    VkPipelineMultisampleStateCreateInfo ms_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState color_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    VkPipelineColorBlendStateCreateInfo blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment
    };

    Array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    auto dynamic_state = vkuCreatePipelineDynamicStateCreateInfo(dynamic_states);

    Array<VkPushConstantRange, 1> push_constants = {
        VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(LinaVGPushConstants)
        }
    };

    auto desc_set_layouts = _renderer->get_descriptor_set_layouts();
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2, // Only use descriptor sets 0 (uniform) and 1 (texture)
        .pSetLayouts = desc_set_layouts.data(),
        .pushConstantRangeCount = push_constants.size(),
        .pPushConstantRanges = push_constants.data(),
    };
    VK_CHECK(vkCreatePipelineLayout(_renderer->get_device(), &layout_info, nullptr, &_pipeline_layout));

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_renderer->get_swapchain_settings().surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
    };

    for (int render_type = 0; render_type < RenderTypeCount; render_type++) {
		VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
	        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	        .pNext = &pipeline_rendering_create_info,
	        .stageCount = shader_stages_per_type[render_type].size(),
	        .pStages = shader_stages_per_type[render_type].data(),
	        .pVertexInputState = &vertex_input_info,
	        .pInputAssemblyState = &ia_info,
	        .pViewportState = &viewport_state,
	        .pRasterizationState = &raster_info,
	        .pMultisampleState = &ms_info,
	        .pDepthStencilState = &depth_info,
	        .pColorBlendState = &blend_info,
	        .pDynamicState = &dynamic_state,
	        .layout = _pipeline_layout,
	        .renderPass = nullptr,
	        .subpass = 0
	    };
	    VK_CHECK(vkCreateGraphicsPipelines(_renderer->get_device(), VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &_pipelines[render_type]));
    }

    _renderer->destroy_shader(shader_vs);
    _renderer->destroy_shader(shader_fs_default);
    _renderer->destroy_shader(shader_fs_textured);
    _renderer->destroy_shader(shader_fs_simple_text);
    _renderer->destroy_shader(shader_fs_sdf_text);
    _renderer->destroy_shader(shader_fs_rounded_gradient);

    constexpr int MAX_VERTICES = 1000000;
    constexpr int MAX_INDICES = 1000000;
    for (int render_type = 0; render_type < RenderTypeCount; render_type++) {
	    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			_renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, i, MAX_VERTICES * sizeof(LinaVG::Vertex), _vertex_buffers[render_type]);
			_renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, i, MAX_INDICES * sizeof(LinaVG::Index), _index_buffers[render_type]);
	    }
	}
}

void LinaVGRenderer::begin_frame() {
	using namespace LinaVG;
	if (Internal::g_rendererData.m_frameStarted) {
		log_error("LinaVG: StartFrame was called, but EndFrame was skipped! Make sure you always call EndFrame() after calling StartFrame() for the second time!");
	}
	Internal::g_rendererData.m_frameStarted = true;
}

void LinaVGRenderer::end_frame() {
	using namespace LinaVG;

	Internal::g_rendererData.m_frameStarted = false;

	Internal::g_rendererData.m_gcFrameCounter++;

	if (Internal::g_rendererData.m_gcFrameCounter > Config.gcCollectInterval)
	{
	    Internal::g_rendererData.m_gcFrameCounter = 0;
	    Internal::ClearAllBuffers();
	    Internal::g_rendererData.m_drawOrders.clear();
	}
	else
	{
	    for (int i = 0; i < Internal::g_rendererData.m_gradientBuffers.m_size; i++)
	        Internal::g_rendererData.m_gradientBuffers[i].ShrinkZero();

	    for (int i = 0; i < Internal::g_rendererData.m_textureBuffers.m_size; i++)
	        Internal::g_rendererData.m_textureBuffers[i].ShrinkZero();

	    for (int i = 0; i < Internal::g_rendererData.m_defaultBuffers.m_size; i++)
	        Internal::g_rendererData.m_defaultBuffers[i].ShrinkZero();

	    for (int i = 0; i < Internal::g_rendererData.m_simpleTextBuffers.m_size; i++)
	        Internal::g_rendererData.m_simpleTextBuffers[i].ShrinkZero();

	    for (int i = 0; i < Internal::g_rendererData.m_sdfTextBuffers.m_size; i++)
	        Internal::g_rendererData.m_sdfTextBuffers[i].ShrinkZero();
	}

	// SDF
	if (Config.textCachingEnabled || Config.textCachingSDFEnabled)
	    Internal::g_rendererData.m_textCacheFrameCounter++;

	if (Internal::g_rendererData.m_textCacheFrameCounter > Config.textCacheExpireInterval)
	{
	    Internal::g_rendererData.m_textCacheFrameCounter = 0;
	    Internal::g_rendererData.m_textCache.clear();
	    Internal::g_rendererData.m_sdfTextCache.clear();
	}
}

void LinaVGRenderer::exec_render_pass(VkCommandBuffer command_buffer, int drawOrder, LinaVG::DrawBufferShapeType shapeType) {
	using namespace LinaVG;

    auto desc_sets = _renderer->get_descriptor_sets_for_current_frame();

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[(int)RenderType::Default]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0,
		1, desc_sets.data(), 0, nullptr);

    for (int i = 0; i < Internal::g_rendererData.m_defaultBuffers.m_size; i++)
    {
        DrawBuffer& buf = Internal::g_rendererData.m_defaultBuffers[i];

        if (buf.m_drawOrder == drawOrder && buf.m_shapeType == shapeType) {
        	render_default(command_buffer, buf);
        }
    }

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[(int)RenderType::RoundedGradient]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0,
		1, desc_sets.data(), 0, nullptr);
    for (int i = 0; i < Internal::g_rendererData.m_gradientBuffers.m_size; i++)
    {
        GradientDrawBuffer& buf = Internal::g_rendererData.m_gradientBuffers[i];

        if (buf.m_drawOrder == drawOrder && buf.m_shapeType == shapeType) {
        	render_gradient(command_buffer, buf);
        }
    }

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[(int)RenderType::Textured]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0,
		2, desc_sets.data(), 0, nullptr);
    for (int i = 0; i < Internal::g_rendererData.m_textureBuffers.m_size; i++)
    {
        TextureDrawBuffer& buf = Internal::g_rendererData.m_textureBuffers[i];

        if (buf.m_drawOrder == drawOrder && buf.m_shapeType == shapeType) {
        	render_texture(command_buffer, buf);
        }
    }

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[(int)RenderType::SimpleText]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0,
		2, desc_sets.data(), 0, nullptr);
    for (int i = 0; i < Internal::g_rendererData.m_simpleTextBuffers.m_size; i++)
    {
        SimpleTextDrawBuffer& buf = Internal::g_rendererData.m_simpleTextBuffers[i];

        if (buf.m_drawOrder == drawOrder && buf.m_shapeType == shapeType) {
        	render_simple_text(command_buffer, buf);
        }
    }

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[(int)RenderType::SDFText]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0,
		2, desc_sets.data(), 0, nullptr);
    for (int i = 0; i < Internal::g_rendererData.m_sdfTextBuffers.m_size; i++)
    {
        SDFTextDrawBuffer& buf = Internal::g_rendererData.m_sdfTextBuffers[i];

        if (buf.m_drawOrder == drawOrder && buf.m_shapeType == shapeType) {
        	render_sdf_text(command_buffer, buf);
        }
    }
}

void LinaVGRenderer::render(VkCommandBuffer command_buffer) {
	using namespace LinaVG;

	TracyVkZone(_renderer->get_current_tracy_graphics_context(), command_buffer, "LinaVGRenderer");

	auto window_extent = _renderer->get_window_extent();
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (float)window_extent.x, .height = (float)window_extent.y,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = { { 0, 0 }, { (uint32_t)window_extent.x, (uint32_t)window_extent.y } };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	auto res = Res::inst();

	auto& arr = Internal::g_rendererData.m_drawOrders;

	for (int i = 0; i < arr.m_size; i++)
	{
	    const int drawOrder = arr[i];
	    exec_render_pass(command_buffer, drawOrder, DrawBufferShapeType::DropShadow);
	    exec_render_pass(command_buffer, drawOrder, DrawBufferShapeType::Shape);
	    exec_render_pass(command_buffer, drawOrder, DrawBufferShapeType::Outline);
	    exec_render_pass(command_buffer, drawOrder, DrawBufferShapeType::AA);
	}
}

void LinaVGRenderer::render_default(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf) {
	if (buf.m_indexBuffer.m_size == 0) return;

	copy_buffer_data_to_gpu(buf, RenderType::Default);
	cmd_bind_buffers(command_buffer, buf, RenderType::Default);
	cmd_set_scissors(command_buffer, buf);

	cmd_draw_indexed(command_buffer, buf);
}

void LinaVGRenderer::render_gradient(VkCommandBuffer command_buffer, LinaVG::GradientDrawBuffer& buf) {
	if (buf.m_indexBuffer.m_size == 0) return;

	copy_buffer_data_to_gpu(buf, RenderType::RoundedGradient);
	cmd_bind_buffers(command_buffer, buf, RenderType::RoundedGradient);
	cmd_set_scissors(command_buffer, buf);

	LinaVGPushConstants pc;
	pc.startColor = glm::make_vec4((const float*)&buf.m_color.start);
	pc.endColor = glm::make_vec4((const float*)&buf.m_color.end);
	pc.gradientType = (int)buf.m_color.gradientType;
	pc.radialSize = buf.m_color.radialSize;
	pc.isAABuffer = (int)buf.m_isAABuffer;

	vkCmdPushConstants(command_buffer, _pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinaVGPushConstants), &pc);

	cmd_draw_indexed(command_buffer, buf);
}

void LinaVGRenderer::render_texture(VkCommandBuffer command_buffer, LinaVG::TextureDrawBuffer& buf) {
	if (buf.m_indexBuffer.m_size == 0) return;

	copy_buffer_data_to_gpu(buf, RenderType::Textured);
	cmd_bind_buffers(command_buffer, buf, RenderType::Textured);
	cmd_set_scissors(command_buffer, buf);

	glm::vec2 uv = LinaVG::Config.flipTextureUVs? 
		glm::vec2(buf.m_textureUVTiling.x, -buf.m_textureUVTiling.y) : glm::vec2(buf.m_textureUVTiling.x, buf.m_textureUVTiling.y);

	LinaVGPushConstants pc;
	pc.diffuse = buf.m_textureHandle;
	pc.tiling = uv;
	pc.offset = {buf.m_textureUVOffset.x, buf.m_textureUVOffset.y};
	pc.tint = glm::make_vec4((const float*)&buf.m_tint);
	pc.isAABuffer = (int)buf.m_isAABuffer;

	vkCmdPushConstants(command_buffer, _pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinaVGPushConstants), &pc);

	cmd_draw_indexed(command_buffer, buf);
}

void LinaVGRenderer::render_simple_text(VkCommandBuffer command_buffer, LinaVG::SimpleTextDrawBuffer& buf) {
	if (buf.m_indexBuffer.m_size == 0) return;

	copy_buffer_data_to_gpu(buf, RenderType::SimpleText);
	cmd_bind_buffers(command_buffer, buf, RenderType::SimpleText);
	cmd_set_scissors(command_buffer, buf);

	LinaVGPushConstants pc;
	pc.diffuse = buf.m_textureHandle;

	vkCmdPushConstants(command_buffer, _pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinaVGPushConstants), &pc);

	cmd_draw_indexed(command_buffer, buf);
}

void LinaVGRenderer::render_sdf_text(VkCommandBuffer command_buffer, LinaVG::SDFTextDrawBuffer& buf) {
	if (buf.m_indexBuffer.m_size == 0) return;

	copy_buffer_data_to_gpu(buf, RenderType::SDFText);
	cmd_bind_buffers(command_buffer, buf, RenderType::SDFText);
	cmd_set_scissors(command_buffer, buf);

	LinaVGPushConstants pc;
	pc.diffuse = buf.m_textureHandle;
	pc.thickness = 1.0f - glm::clamp(buf.m_thickness, 0.0f, 1.0f);
	pc.softness = glm::clamp(buf.m_softness, 0.0f, 10.0f) * 0.1f;
	pc.outlineThickness = glm::clamp(buf.m_outlineThickness, 0.0f, 1.0f);
	pc.outlineColor = glm::make_vec4((const float*)&buf.m_outlineColor);
	pc.outlineEnabled = pc.outlineThickness != 0.0f ? 1 : 0;
	pc.flipAlpha = buf.m_flipAlpha? 1 : 0;

	vkCmdPushConstants(command_buffer, _pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinaVGPushConstants), &pc);

	cmd_draw_indexed(command_buffer, buf);
}

void LinaVGRenderer::copy_buffer_data_to_gpu(LinaVG::DrawBuffer& buf, RenderType render_type) {
	uint32_t cur_frame = _renderer->get_current_frame();
	size_t vertex_size = buf.m_vertexBuffer.m_size * sizeof(LinaVG::Vertex);
	size_t index_size = buf.m_indexBuffer.m_size * sizeof(LinaVG::Index);
	auto& vertex_buffer = _vertex_buffers[(int)render_type];
	auto& index_buffer = _index_buffers[(int)render_type];
	_renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, cur_frame, vertex_size, vertex_buffer);
	_renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, cur_frame, index_size, index_buffer);
	auto vtx_dst = _renderer->get_mapped_pointer(vertex_buffer, cur_frame);
	auto idx_dst = _renderer->get_mapped_pointer(index_buffer, cur_frame);
	memcpy(vtx_dst, buf.m_vertexBuffer.m_data, vertex_size);
	memcpy(idx_dst, buf.m_indexBuffer.m_data, index_size);
}

void LinaVGRenderer::cmd_bind_buffers(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf, RenderType render_type) {
	uint32_t cur_frame = _renderer->get_current_frame();
	auto& vertex_buffer = _vertex_buffers[(int)render_type];
	auto& index_buffer = _index_buffers[(int)render_type];
	vkuCmdBindSingleVertexBuffer(command_buffer, vertex_buffer.buffer_per_frame[cur_frame].buffer);
	vkCmdBindIndexBuffer(command_buffer, index_buffer.buffer_per_frame[cur_frame].buffer,
		0, sizeof(LinaVG::Index) == 2? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void LinaVGRenderer::cmd_set_scissors(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf) {
	// TODO: recheck how scissor bounds are calculated (currently different from LinaVG's own renderer)
	VkRect2D scissor;
	if (buf.clipSizeX == 0 || buf.clipSizeY == 0) {
		scissor.offset.x = (int32_t)LinaVG::Config.displayPosX;
		scissor.offset.y = (int32_t)LinaVG::Config.displayPosY;
		scissor.extent.width = (int32_t)LinaVG::Config.displayWidth;
		scissor.extent.height = (int32_t)LinaVG::Config.displayHeight;
	}
	else {
		scissor.offset.x = (int32_t)buf.clipPosX;
		scissor.offset.y = (int32_t)buf.clipPosY;
		scissor.extent.width = (uint32_t)buf.clipSizeX;
		scissor.extent.height = (uint32_t)buf.clipSizeY;
	}
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void LinaVGRenderer::cmd_draw_indexed(VkCommandBuffer command_buffer, LinaVG::DrawBuffer& buf) {
	vkCmdDrawIndexed(command_buffer, buf.m_indexBuffer.m_size, 1, 0, 0, 0);

	LinaVG::Config.debugCurrentDrawCalls++;
	LinaVG::Config.debugCurrentTriangleCount += int((float)buf.m_indexBuffer.m_size / 3.0f);
	LinaVG::Config.debugCurrentVertexCount += buf.m_vertexBuffer.m_size;
}
