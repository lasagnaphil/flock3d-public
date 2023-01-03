#include "mesh_renderer.h"

#include "renderer.h"
#include "vk_utils.h"

#include "res.h"
#include "vulkan/vulkan_core.h"

#define TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"

void MeshRenderer::init() {
	create_graphics_pipeline();
}

void MeshRenderer::create_graphics_pipeline() {
	Shader vert_shader = _renderer->load_shader_from_file("shaders/textured_mesh.vert.spv", ShaderType::Vertex);
    Shader frag_shader = _renderer->load_shader_from_file("shaders/textured_mesh.frag.spv", ShaderType::Fragment);

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader.stage_info(), frag_shader.stage_info()};

    Array<VkVertexInputBindingDescription, 1> binding_descs = {
            VkVertexInputBindingDescription {
                    .binding = 0,
                    .stride = sizeof(TexturedVertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
    };
    Array<VkVertexInputAttributeDescription, 3> attribute_descs = {
            VkVertexInputAttributeDescription {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(TexturedVertex, pos)
            },
            VkVertexInputAttributeDescription {
                    .location = 1,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(TexturedVertex, normal)
            },
            VkVertexInputAttributeDescription {
                    .location = 2,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32_SFLOAT,
                    .offset = offsetof(TexturedVertex, texcoord)
            },
    };

    auto vertex_input_info = vkuCreatePipelineVertexInputStateCreateInfo(binding_descs, attribute_descs);

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
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
            .size = sizeof(MeshPushConstants),
    };

    auto desc_set_layouts = _renderer->get_descriptor_set_layouts();

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = desc_set_layouts.size(),
            .pSetLayouts = desc_set_layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant
    };
    VK_CHECK(vkCreatePipelineLayout(_renderer->get_device(), &pipeline_layout_info, nullptr, &_graphics_pipeline_layout));

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_renderer->get_swapchain_settings().surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
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

    VK_CHECK(vkCreateGraphicsPipelines(_renderer->get_device(), VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &_graphics_pipeline));

    vkDestroyShaderModule(_renderer->get_device(), vert_shader.module, nullptr);
    vkDestroyShaderModule(_renderer->get_device(), frag_shader.module, nullptr);

}

void MeshRenderer::render(VkCommandBuffer command_buffer) {
    TracyVkZone(_renderer->get_current_tracy_graphics_context(), command_buffer, "MeshRenderer")

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    auto& camera = _renderer->get_current_camera();

    auto descriptor_sets = _renderer->get_descriptor_sets_for_current_frame();

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline_layout, 0,
                            descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);

    MeshPushConstants push_constants;
    push_constants.cam_pos = glm::vec3(camera.get_view_matrix()[3]);
    
    _ecs->query<Model, Transform>().foreach([&](Entity entity, Model& model, const Transform& transform) {
        auto res = Res::inst();

        push_constants.view_model = camera.get_view_matrix() * transform.to_matrix();
        push_constants.color = glm::vec4(1, 1, 1, 1);

        for (int i = 0; i < model.meshes.size(); i++) {
        	auto mesh_id = model.meshes[i];
	        TexturedMesh* mesh = res->get(mesh_id);

            push_constants.mat_id = res->_material_pool.get_item_idx(mesh->mat_id);
            vkCmdPushConstants(command_buffer, _graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &push_constants);
	        vkuCmdBindSingleVertexBuffer(command_buffer, mesh->vertex_buffer.buffer);
	        if (mesh->index_count > 0) {
	            vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
	        }

	        if (mesh->index_count > 0) {
	            vkCmdDrawIndexed(command_buffer, mesh->index_count, 1, 0, 0, 0);
	        }
	        else {
	            vkCmdDraw(command_buffer, mesh->vertex_count, 1, 0, 0);
	        }
        }

    });
}

void MeshRenderer::cleanup() {
}
