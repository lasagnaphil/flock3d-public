#include "terrain.h"

#include "imgui.h"

#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/hash.hpp>

#include "render/renderer.h"
#include "render/im3d_renderer.h"

#include "terrain_algo.h"
#include "vk_utils.h"

#include "engine.h"

#define TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"

constexpr int MAX_CHUNKS_PER_TEMPLATE = 1000;

TerrainRenderer::TerrainRenderer(Renderer *renderer, Terrain* terrain) 
    : RenderInterface(renderer), _terrain(terrain) {}

void TerrainRenderer::init() {
    _chunk_templates.resize(_terrain->chunk_sizes.size());

    for (int k = 0; k < _terrain->chunk_sizes.size(); k++) {
        auto &chunk = _chunk_templates[k];
        int N;
        N = chunk.grid_size = _terrain->chunk_sizes[k];
        chunk.uvs.resize((N + 1) * (N + 1));
        for (int j = 0; j <= N; j++) {
            for (int i = 0; i <= N; i++) {
                chunk.uvs[j * (N + 1) + i] = glm::vec2((float) i / (float) N, (float) j / (float) N);
            }
        }
        chunk.triangles.resize(2 * N * N);
        for (int j = 0; j < N; j++) {
            for (int i = 0; i < N; i++) {
                uint32_t v1 = j * (N + 1) + i;
                uint32_t v2 = j * (N + 1) + i + 1;
                uint32_t v3 = (j + 1) * (N + 1) + i;
                uint32_t v4 = (j + 1) * (N + 1) + i + 1;
                chunk.triangles[2 * (j * N + i) + 0] = {v1, v3, v2};
                chunk.triangles[2 * (j * N + i) + 1] = {v2, v3, v4};
            }
        }
    }
    for (auto& chunk : _chunk_templates) {
        chunk.vbo_inst = _renderer->create_dynamic_render_buffer(
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(glm::vec2) * MAX_CHUNKS_PER_TEMPLATE);
    }

    auto command_buffer = _renderer->begin_single_time_commands();
    for (auto& chunk : _chunk_templates) {
        _renderer->create_static_render_buffer_from_cpu(command_buffer,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, chunk.uvs.data(), sizeof(glm::vec2) * chunk.uvs.size(),
                chunk.vbo_staging_buffer, chunk.vbo);
        _renderer->create_static_render_buffer_from_cpu(command_buffer,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, chunk.triangles.data(), sizeof(glm::u16vec3) * chunk.triangles.size(),
                chunk.ibo_staging_buffer, chunk.ibo);
    }
    _renderer->end_single_time_commands(command_buffer);
    for (auto& chunk : _chunk_templates) {
        _renderer->destroy_buffer(chunk.vbo_staging_buffer);
        _renderer->destroy_buffer(chunk.ibo_staging_buffer);
    }

    create_graphics_pipeline();
}

void TerrainRenderer::create_graphics_pipeline() {
    Shader vs_shader = _renderer->load_shader_from_file("shaders/terrain.vert.spv", ShaderType::Vertex);
    Shader fs_shader = _renderer->load_shader_from_file("shaders/terrain.frag.spv", ShaderType::Fragment);

    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_shader.stage_info(), fs_shader.stage_info()};

    Array<VkVertexInputBindingDescription, 2> binding_descs = {
        VkVertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(glm::vec2),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        },
        VkVertexInputBindingDescription {
            .binding = 1,
            .stride = sizeof(glm::vec2),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
        }
    };
    Array<VkVertexInputAttributeDescription, 2> attribute_descs = {
        VkVertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0
        },
        VkVertexInputAttributeDescription {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0
        }
    };

    auto vertex_input_info = vkuCreatePipelineVertexInputStateCreateInfo(binding_descs, attribute_descs);

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = _renderer->default_viewport();
    VkRect2D scissor = _renderer->default_scissor();

    auto viewport_state = vkuCreatePipelineViewportStateCreateInfo(&viewport, &scissor);

    VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
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
            .size = sizeof(TerrainPushConstants),
    };

    auto desc_set_layouts = _renderer->get_descriptor_set_layouts();

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = desc_set_layouts.size(),
            .pSetLayouts = desc_set_layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant
    };
    VK_CHECK(vkCreatePipelineLayout(_renderer->_device, &pipeline_layout_info, nullptr, &_graphics_pipeline_layout));

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
            .pDynamicState = nullptr,
            .layout = _graphics_pipeline_layout,
            .renderPass = nullptr,
            .subpass = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(_renderer->_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &_graphics_pipeline));

    vkDestroyShaderModule(_renderer->_device, vs_shader.module, nullptr);
    vkDestroyShaderModule(_renderer->_device, fs_shader.module, nullptr);
}

void TerrainRenderer::begin_frame() {
    auto engine = Engine::instance();
    auto im3d = engine->im3d.get();

    const Camera& camera = _renderer->get_current_camera();

    auto pos_xy = glm::vec2(camera.position.x, camera.position.z);
    auto ipos = glm::ivec2(glm::floor(pos_xy / _terrain->chunk_width));

    // test
    _chunks.clear();
    for (int j = -10; j <= 10; j++) {
        for (int i = -10; i <= 10; i++) {
            int detail_level;
            if (i * i + j * j < 2 * 2) detail_level = 2;
            else if (i * i + j * j < 6 * 6) detail_level = 1;
            else detail_level = 0;
            glm::ivec2 coords = ipos + glm::ivec2(i, j);
            _chunks.push_back({coords, detail_level});
        }
    }
}

void TerrainRenderer::render(VkCommandBuffer command_buffer) {
    TracyVkZone(_renderer->get_current_tracy_graphics_context(), command_buffer, "TerrainRenderer")
    const Camera& camera = _renderer->get_current_camera();

    TerrainPushConstants push_constants;
    push_constants.set_config(*_terrain);
    push_constants.view = camera.get_view_matrix();

    Vector<Vector<glm::vec2>> chunk_groups(_chunk_templates.size());
    for (auto& chunk : _chunks) {
        chunk_groups[chunk.tmpl_idx].push_back(chunk.pos);
    }

    auto descriptor_sets = _renderer->get_descriptor_sets_for_current_frame();

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline_layout, 0,
                            descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);

    vkCmdPushConstants(command_buffer, _graphics_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TerrainPushConstants), &push_constants);

    for (int tmpl_idx = 0; tmpl_idx < _chunk_templates.size(); tmpl_idx++) {
        auto& chunk_tmpl = _chunk_templates[tmpl_idx];
        auto& chunk_positions = chunk_groups[tmpl_idx];

        // Update offset buffer for instancing
        void* p_chunk_vbo = _renderer->get_mapped_pointer(chunk_tmpl.vbo_inst);
        memcpy(p_chunk_vbo, chunk_positions.data(), sizeof(glm::vec2) * chunk_positions.size());

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &chunk_tmpl.vbo.buffer, offsets);
        vkCmdBindVertexBuffers(command_buffer, 1, 1, &chunk_tmpl.vbo_inst.buffer, offsets);
        vkCmdBindIndexBuffer(command_buffer, chunk_tmpl.ibo.buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(command_buffer, 3 * chunk_tmpl.triangles.size(), chunk_positions.size(), 0, 0, 0);
    }
}

void TerrainRenderer::cleanup() {
    for (auto& chunk_tmpl : _chunk_templates) {
        _renderer->destroy_buffer(chunk_tmpl.vbo);
        _renderer->destroy_buffer(chunk_tmpl.vbo_inst);
        _renderer->destroy_buffer(chunk_tmpl.ibo);
        chunk_tmpl.uvs.clear();
        chunk_tmpl.triangles.clear();
    }
}

