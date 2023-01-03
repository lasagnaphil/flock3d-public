#include "im3d_renderer.h"

#include "vk_utils.h"

#define TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"

#define ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*(_ARR))))     // Size of a static C-style array. Don't use on pointers!

void Im3dRenderer::init() {
    _pipeline_data[(int)PrimType::Point].max_prim_count = 1000000;
    _pipeline_data[(int)PrimType::Point].max_batch_count = 10000;
    _pipeline_data[(int)PrimType::Line].max_prim_count = 1000000;
    _pipeline_data[(int)PrimType::Line].max_batch_count = 10000;
    _pipeline_data[(int)PrimType::Triangle].max_prim_count = 1000000;
    _pipeline_data[(int)PrimType::Triangle].max_batch_count = 10000;

    create_buffers();
    create_graphics_pipelines();

    _is_initialized = true;
}

void Im3dRenderer::create_graphics_pipelines() {
    for (PrimType primtype : {PrimType::Point, PrimType::Line, PrimType::Triangle}) {
        Shader vs_shader, fs_shader;

        Array<VkDynamicState, 1> dynamic_states = { VK_DYNAMIC_STATE_LINE_WIDTH };
        VkPipelineDynamicStateCreateInfo dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = dynamic_states.size(),
                .pDynamicStates = dynamic_states.data()
        };

        auto& pl = _pipeline_data[(int)primtype];

        switch (primtype) {
            case PrimType::Point: {
                vs_shader = _renderer->load_shader_from_file("shaders/im3d_point.vert.spv", ShaderType::Vertex);
                fs_shader = _renderer->load_shader_from_file("shaders/im3d_point.frag.spv", ShaderType::Fragment);
            } break;
            case PrimType::Line: {
                vs_shader = _renderer->load_shader_from_file("shaders/im3d_line.vert.spv", ShaderType::Vertex);
                fs_shader = _renderer->load_shader_from_file("shaders/im3d_line.frag.spv", ShaderType::Fragment);
            } break;
            case PrimType::Triangle: {
                vs_shader = _renderer->load_shader_from_file("shaders/im3d_tri.vert.spv", ShaderType::Vertex);
                fs_shader = _renderer->load_shader_from_file("shaders/im3d_tri.frag.spv", ShaderType::Fragment);
            } break;
        }

        VkPipelineShaderStageCreateInfo shader_stages[] = {vs_shader.stage_info(), fs_shader.stage_info()};

        Array<VkVertexInputBindingDescription, 1> binding_descs = {
            VkVertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };

        Array<VkVertexInputAttributeDescription, 1> attribute_descs = {
            VkVertexInputAttributeDescription {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            }
        };

        auto vertex_input_info = vkuCreatePipelineVertexInputStateCreateInfo(binding_descs, attribute_descs);

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        if (primtype == PrimType::Point) {
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }
        else if (primtype == PrimType::Line) {
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }
        else if (primtype == PrimType::Triangle) {
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }

        VkViewport viewport = _renderer->default_viewport();
        VkRect2D scissor = _renderer->default_scissor();

        auto viewport_state = vkuCreatePipelineViewportStateCreateInfo(&viewport, &scissor);

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0f,
                .depthBiasClamp = 0.0f,
                .depthBiasSlopeFactor = 0.0f,
                .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
                .minSampleShading = 1.0f,
                .pSampleMask = nullptr,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS,
                .depthBoundsTestEnable = VK_FALSE,
                .minDepthBounds = 0.0f,
                .maxDepthBounds = 1.0f,
                .stencilTestEnable = VK_FALSE,
                .front = {},
                .back = {}
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blending = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment,
                .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
        };

        VkPushConstantRange push_constant = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(Im3dPushConstants),
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                .pSetLayouts = nullptr,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constant
        };
        VK_CHECK(vkCreatePipelineLayout(_renderer->_device, &pipeline_layout_info, nullptr, &pl.graphics_pipeline_layout));

        VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &_renderer->get_swapchain_settings().surface_format.format,
                .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
        };

        VkGraphicsPipelineCreateInfo pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &pipeline_rendering_create_info,
                .stageCount = 2,
                .pStages = shader_stages,
                .pVertexInputState = &vertex_input_info,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state,
                .layout = pl.graphics_pipeline_layout,
                .renderPass = nullptr,
                .subpass = 0,
        };

        VK_CHECK(vkCreateGraphicsPipelines(_renderer->_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pl.graphics_pipeline));

        vkDestroyShaderModule(_renderer->_device, vs_shader.module, nullptr);
        vkDestroyShaderModule(_renderer->_device, fs_shader.module, nullptr);
    }
}

void Im3dRenderer::create_buffers() {
    auto& point_data = _pipeline_data[(int)PrimType::Point];
    auto& line_data = _pipeline_data[(int)PrimType::Line];
    auto& tri_data = _pipeline_data[(int)PrimType::Triangle];

    point_data.vbo = _renderer->create_dynamic_render_buffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, point_data.max_prim_count * sizeof(glm::vec3));
    line_data.vbo = _renderer->create_dynamic_render_buffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 2*line_data.max_prim_count * sizeof(glm::vec3));
    tri_data.vbo = _renderer->create_dynamic_render_buffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 3*tri_data.max_prim_count * sizeof(glm::vec3));
}

void Im3dRenderer::new_frame() {
    auto& point_data = _pipeline_data[(int)PrimType::Point];
    auto& line_data = _pipeline_data[(int)PrimType::Line];
    auto& tri_data = _pipeline_data[(int)PrimType::Triangle];

    for (auto& data : _pipeline_data) {
        data.positions.clear();
        data.positions.reserve(data.max_prim_count);
        data.batch_data.clear();
        data.batch_data.reserve(data.max_batch_count);
        data.batch_starts.clear();
        data.batch_starts.reserve(data.max_batch_count);
        data.batch_starts.push_back(0);
        data.cur_batch_data = {
                .color = {1, 1, 1, 1},
                .prim_width = 1.0f,
                .blend_factor = 1.5f,
        };
    }
}

void Im3dRenderer::render(VkCommandBuffer command_buffer) {
    TracyVkZone(_renderer->get_current_tracy_graphics_context(), command_buffer, "Im3dRenderer")

    const Camera& camera = _renderer->get_current_camera();

    // If current batch isn't empty, then add to batch list now
    start_new_point_batch();
    start_new_line_batch();
    start_new_tri_batch();

    // If there are no batches, then skip render
    for (PrimType primtype : {PrimType::Point, PrimType::Line, PrimType::Triangle}) {
        auto& data = get_pipeline_data(primtype);
        if (data.batch_data.empty()) continue;

        // Copy line vertex positions to vbo
        void* p_vbo = _renderer->get_mapped_pointer(data.vbo);
        memcpy(p_vbo, data.positions.data(), sizeof(glm::vec3) * data.positions.size());

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.graphics_pipeline);

        VkBuffer vertex_buffers[] = {data.vbo.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

        for (int batch_idx = 0; batch_idx < data.batch_data.size(); batch_idx++) {
            auto& push_constants = data.batch_data[batch_idx];
            push_constants.projview = camera.proj_mat * camera.get_view_matrix();
            push_constants.viewport_size = _renderer->get_window_extent();
            uint32_t idx_start = data.batch_starts[batch_idx];
            uint32_t idx_end = data.batch_starts[batch_idx + 1];
            uint32_t num_prims = idx_end - idx_start;
            vkCmdPushConstants(command_buffer, data.graphics_pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Im3dPushConstants), &push_constants);
            vkCmdSetLineWidth(command_buffer, push_constants.prim_width);
            if (primtype == PrimType::Point) {
                vkCmdDraw(command_buffer, num_prims, 1, idx_start, 0);
            }
            else if (primtype == PrimType::Line) {
                vkCmdDraw(command_buffer, 2*num_prims, 1, 2*idx_start, 0);
            }
            else if (primtype == PrimType::Triangle) {
                vkCmdDraw(command_buffer, 3*num_prims, 1, 3*idx_start, 0);
            }
        }
    }

}

void Im3dRenderer::cleanup() {
    if (_is_initialized) {
        _renderer->destroy_buffer(get_pipeline_data(PrimType::Point).vbo);
        _renderer->destroy_buffer(get_pipeline_data(PrimType::Line).vbo);
        _renderer->destroy_buffer(get_pipeline_data(PrimType::Triangle).vbo);
        _is_initialized = false;
    }
}

void Im3dRenderer::set_point_color(glm::vec4 color) {
    auto& cur_color = get_pipeline_data(PrimType::Point).cur_batch_data.color;
    if (cur_color != color) {
        start_new_line_batch();
        cur_color = color;
    }
}

void Im3dRenderer::set_point_size(float size) {
    auto& cur_width = get_pipeline_data(PrimType::Point).cur_batch_data.prim_width;
    if (cur_width != size) {
        start_new_line_batch();
        cur_width = size;
    }
}

void Im3dRenderer::set_point_blend_factor(float blend_factor) {
    auto& cur_blend_factor = get_pipeline_data(PrimType::Point).cur_batch_data.blend_factor;
    if (cur_blend_factor != blend_factor) {
        start_new_point_batch();
        cur_blend_factor = blend_factor;
    }
}

void Im3dRenderer::set_line_color(glm::vec4 color) {
    auto& cur_color = get_pipeline_data(PrimType::Line).cur_batch_data.color;
    if (cur_color != color) {
        start_new_line_batch();
        cur_color = color;
    }
}

void Im3dRenderer::set_line_width(float width) {
    auto& cur_width = get_pipeline_data(PrimType::Line).cur_batch_data.prim_width;
    if (cur_width != width) {
        start_new_line_batch();
        cur_width = width;
    }
}

void Im3dRenderer::set_line_blend_factor(float blend_factor) {
    auto& cur_blend_factor = get_pipeline_data(PrimType::Line).cur_batch_data.blend_factor;
    if (cur_blend_factor != blend_factor) {
        start_new_line_batch();
        cur_blend_factor = blend_factor;
    }
}

void Im3dRenderer::set_tri_color(glm::vec4 color) {
    auto& cur_color = get_pipeline_data(PrimType::Triangle).cur_batch_data.color;
    if (cur_color != color) {
        start_new_tri_batch();
        cur_color = color;
    }
}

void Im3dRenderer::push_point(const glm::vec3 &p) {
    get_pipeline_data(PrimType::Point).positions.push_back(p);
}

void Im3dRenderer::push_line(const glm::vec3 &p1, const glm::vec3 &p2) {
    auto& line_positions = get_pipeline_data(PrimType::Line).positions;
    line_positions.push_back(p1);
    line_positions.push_back(p2);
}

void Im3dRenderer::push_tri(const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3) {
    auto& tri_positions = get_pipeline_data(PrimType::Triangle).positions;
    tri_positions.push_back(p1);
    tri_positions.push_back(p2);
    tri_positions.push_back(p3);
}

void Im3dRenderer::start_new_batch(PrimType primtype) {
    // only create new batch if current batch is not empty
    auto& point_data = get_pipeline_data(primtype);
    if (point_data.batch_starts.back() != point_data.positions.size()) {
        point_data.batch_starts.push_back(point_data.positions.size());
        point_data.batch_data.push_back(point_data.cur_batch_data);
    }
}
