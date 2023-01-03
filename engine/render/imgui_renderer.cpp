#include "imgui_renderer.h"

#include <imgui_impl_vulkan.h>
#include "render/renderer.h"
#include "vk_utils.h"

#include "res.h"
#include "vulkan/vulkan_core.h"

#define TRACY_ENABLE
#include "tracy/TracyVulkan.hpp"

void ImGuiRenderer::init() {
    auto res = Res::inst();
    {
        // Create font texture
        Buffer staging_buffer;
        VkCommandBuffer command_buffer = _renderer->begin_single_time_commands();

        ImGuiIO& io = ImGui::GetIO();

        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        auto [image_id, image] = res->_image_pool.emplace();
        ImageCpuData img_cpu;
        img_cpu.channels = 4;
        img_cpu.data_channels = 4;
        io.Fonts->GetTexDataAsRGBA32(&img_cpu.pixels, &img_cpu.width, &img_cpu.height);
        _renderer->upload_to_gpu(command_buffer, img_cpu, format, *image, staging_buffer);

        _renderer->end_single_time_commands(command_buffer);
        _renderer->destroy_buffer(staging_buffer);
        img_cpu.pixels = nullptr;
        img_cpu.cleanup();

        VkSamplerCreateInfo sampler_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .minLod = -1000,
            .maxLod = 1000,
            .maxAnisotropy = 1.0f,
        };

        auto texture_id = _renderer->create_texture(*image, format, sampler_create_info);
        io.Fonts->SetTexID(texture_id.to_userpointer());
    }

    // Create pipeline
    Shader vs_shader = _renderer->load_shader_from_file("shaders/imgui.vert.spv", ShaderType::Vertex);
    Shader fs_shader = _renderer->load_shader_from_file("shaders/imgui.frag.spv", ShaderType::Fragment);

    Array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vs_shader.stage_info(), fs_shader.stage_info()};

    Array<VkVertexInputBindingDescription, 1> binding_descs = {
        VkVertexInputBindingDescription {
            .stride = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    Array<VkVertexInputAttributeDescription, 3> attribute_descs = {
        VkVertexInputAttributeDescription {
            .location = 0,
            .binding = binding_descs[0].binding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, pos)
        },
        VkVertexInputAttributeDescription {
            .location = 1,
            .binding = binding_descs[0].binding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(ImDrawVert, uv)
        },
        VkVertexInputAttributeDescription {
            .location = 2,
            .binding = binding_descs[0].binding,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = offsetof(ImDrawVert, col)
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
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
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
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(ImGuiPushConstants)
        }
    };

    auto texture_desc_set_layout = _renderer->get_texture_descriptor_set_layout();
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &texture_desc_set_layout,
        .pushConstantRangeCount = push_constants.size(),
        .pPushConstantRanges = push_constants.data(),
    };
    vkCreatePipelineLayout(_renderer->get_device(), &layout_info, nullptr, &_pipeline_layout);

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_renderer->get_swapchain_settings().surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_create_info,
        .stageCount = shader_stages.size(),
        .pStages = shader_stages.data(),
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

    vkCreateGraphicsPipelines(_renderer->get_device(), VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &_graphics_pipeline);

    vkDestroyShaderModule(_renderer->get_device(), vs_shader.module, nullptr);
    vkDestroyShaderModule(_renderer->get_device(), fs_shader.module, nullptr);

    _is_initialized = true;
}

void ImGuiRenderer::new_frame() {
}

void ImGuiRenderer::render(VkCommandBuffer command_buffer) {
    TracyVkZone(_renderer->get_current_tracy_graphics_context(), command_buffer, "ImGuiRenderer")

    auto res = Res::inst();
 
    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    uint32_t cur_frame = _renderer->get_current_frame();

    if (draw_data->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        _renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, cur_frame, vertex_size, _vertex_buffer);
        _renderer->create_or_resize_dynamic_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, cur_frame, index_size, _index_buffer);
        ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(_renderer->get_mapped_pointer(_vertex_buffer, cur_frame));
        ImDrawIdx* idx_dst = static_cast<ImDrawIdx*>(_renderer->get_mapped_pointer(_index_buffer, cur_frame));
        // Upload vertex/index data into a single contiguous GPU buffer
        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
    }

    setup_render_state(command_buffer);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    setup_render_state(command_buffer);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                vkCmdSetScissor(command_buffer, 0, 1, &scissor);

                Ref<Texture> tex_id = Ref<Texture>::from_userpointer(pcmd->TextureId);

                ImGuiPushConstants pc;
                pc.scale = {2.f / draw_data->DisplaySize.x, 2.f / draw_data->DisplaySize.y};
                pc.translate = {-1.0f - draw_data->DisplayPos.x * pc.scale.x, -1.0f - draw_data->DisplayPos.y * pc.scale.y};
                pc.tex_id = res->_texture_pool.get_item_idx(tex_id);
                vkCmdPushConstants(command_buffer, _pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ImGuiPushConstants), &pc);

                // Draw
                vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitely set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    VkRect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void ImGuiRenderer::cleanup() {
    if (_renderer->_is_initialized) {
        _renderer->_is_initialized = false;
    }
}

void ImGuiRenderer::setup_render_state(VkCommandBuffer command_buffer) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    // Setup desired Vulkan state
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    VkDescriptorSet cur_texture_desc_set = _renderer->get_texture_descriptor_set().set_per_frame[_renderer->get_current_frame()];
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0, 1, &cur_texture_desc_set, 0, nullptr);

    uint32_t cur_frame = _renderer->get_current_frame();

    if (draw_data->TotalVtxCount > 0) {
        vkuCmdBindSingleVertexBuffer(command_buffer, _vertex_buffer.buffer_per_frame[cur_frame].buffer);
        vkCmdBindIndexBuffer(command_buffer, _index_buffer.buffer_per_frame[cur_frame].buffer, 
            0, sizeof(ImDrawIdx) == 2? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // Setup viewport
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (float)fb_width, .height = (float)fb_height,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

