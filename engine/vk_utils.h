#pragma once

#include "core/vector.h"
#include "core/array.h"

#include <volk.h>

#include "core/log.h"
#include "vulkan/vulkan_core.h"


inline std::string_view vk_error_string(VkResult errorCode)
{
    switch (errorCode)
    {
#define STR(r) case VK_ ##r: return #r
        STR(SUCCESS);
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_FRAGMENTED_POOL);
        STR(ERROR_UNKNOWN);
// Provided VK_VERSION_1_1
        STR(ERROR_OUT_OF_POOL_MEMORY);
// Provided VK_VERSION_1_1
        STR(ERROR_INVALID_EXTERNAL_HANDLE);
// Provided VK_VERSION_1_2
        STR(ERROR_FRAGMENTATION);
// Provided VK_VERSION_1_2
        STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
// Provided VK_VERSION_1_3
        STR(PIPELINE_COMPILE_REQUIRED);
// Provided VK_KHR_surface
        STR(ERROR_SURFACE_LOST_KHR);
// Provided VK_KHR_surface
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
// Provided VK_KHR_swapchain
        STR(SUBOPTIMAL_KHR);
// Provided VK_KHR_swapchain
        STR(ERROR_OUT_OF_DATE_KHR);
// Provided VK_KHR_display_swapchain
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
// Provided VK_EXT_debug_report
        STR(ERROR_VALIDATION_FAILED_EXT);
// Provided VK_NV_glsl_shader
        STR(ERROR_INVALID_SHADER_NV);
        /*
// Provided VK_KHR_video_queue
        STR(ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
// Provided VK_KHR_video_queue
        STR(ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
// Provided VK_KHR_video_queue
        STR(ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
// Provided VK_KHR_video_queue
        STR(ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
// Provided VK_KHR_video_queue
        STR(ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
// Provided VK_KHR_video_queue
        STR(ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
         */
// Provided VK_EXT_image_drm_format_modifier
        STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
// Provided VK_KHR_global_priority
        STR(ERROR_NOT_PERMITTED_KHR);
// Provided VK_EXT_full_screen_exclusive
        STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
// Provided VK_KHR_deferred_host_operations
        STR(THREAD_IDLE_KHR);
// Provided VK_KHR_deferred_host_operations
        STR(THREAD_DONE_KHR);
// Provided VK_KHR_deferred_host_operations
        STR(OPERATION_DEFERRED_KHR);
// Provided VK_KHR_deferred_host_operations
        STR(OPERATION_NOT_DEFERRED_KHR);
// Provided VK_EXT_image_compression_control
        STR(ERROR_COMPRESSION_EXHAUSTED_EXT);
// Provided VK_KHR_maintenance1
        /*
        STR(ERROR_OUT_OF_POOL_MEMORY_KHR);
// Provided VK_KHR_external_memory
        STR(ERROR_INVALID_EXTERNAL_HANDLE_KHR);
// Provided VK_EXT_descriptor_indexing
        STR(ERROR_FRAGMENTATION_EXT);
// Provided VK_EXT_global_priority
        STR(ERROR_NOT_PERMITTED_EXT);
// Provided VK_EXT_buffer_device_address
        STR(ERROR_INVALID_DEVICE_ADDRESS_EXT);
// Provided VK_KHR_buffer_device_address
        STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR);
// Provided VK_EXT_pipeline_creation_cache_control
        STR(PIPELINE_COMPILE_REQUIRED_EXT);
// Provided VK_EXT_pipeline_creation_cache_control
        STR(ERROR_PIPELINE_COMPILE_REQUIRED_EXT);
        */
#undef STR
        default:
            return "UNKNOWN_ERROR";
    }
}


#define VK_CHECK(res) { VkResult __result = res; if (__result) { log_error("Detected Vulkan error: {}", vk_error_string(__result)); std::abort(); }}

inline VkImageView vkuCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {
                    .aspectMask = aspect_flags,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &image_view));
    return image_view;
}

/*
struct VkuGraphicsPipelineCreateInfo {
    Vector<VkVertexInputBindingDescription> binding_descs;
    Vector<VkVertexInputAttributeDescription> attribute_descs;
    VkPipelineVertexInputStateCreateInfo vertex_info;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
    VkPipelineViewportStateCreateInfo viewport_info;
    VkPipelineRasterizationStateCreateInfo raster_info;
    VkPipelineMultisampleStateCreateInfo multisample_info;
    Vector<VkPipelineColorBlendAttachmentState> color_attachments;
    Vector<VkPipelineDepthStencilStateCreateInfo> depth_info;
}

inline VkGraphicsPipelineCreateInfo vkuDefaultGraphicsPipelineCreateInfo() {
    return {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

    }
}
*/

inline VkPipelineVertexInputStateCreateInfo vkuCreatePipelineVertexInputStateCreateInfo(
        Span<VkVertexInputBindingDescription> binding_descs,
        Span<VkVertexInputAttributeDescription> attribute_descs) {

    return VkPipelineVertexInputStateCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = binding_descs.size(),
        .pVertexBindingDescriptions = binding_descs.data(),
        .vertexAttributeDescriptionCount = attribute_descs.size(),
        .pVertexAttributeDescriptions = attribute_descs.data(),
    };
}

inline VkPipelineViewportStateCreateInfo vkuCreatePipelineViewportStateCreateInfo(const VkViewport* viewport, const VkRect2D* scissor) {
    return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = viewport,
            .scissorCount = 1,
            .pScissors = scissor
    };
}

inline VkPipelineDynamicStateCreateInfo vkuCreatePipelineDynamicStateCreateInfo(Span<VkDynamicState> dynamic_states) {
    return VkPipelineDynamicStateCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_states.size(),
        .pDynamicStates = dynamic_states.data()
    };
}

inline void vkuCreateDescriptorSetLayout(VkDevice device, Span<VkDescriptorSetLayoutBinding> bindings, VkDescriptorSetLayout* layout, void* pNext = nullptr) {
    VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = pNext,
            .flags = 0,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, layout));
}

inline void vkuCreateDescriptorSets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, Span<VkDescriptorSet> descriptor_sets, void* pNext = nullptr) {
    Vector<VkDescriptorSetLayout> layouts(descriptor_sets.size());
    for (uint32_t i = 0; i < descriptor_sets.size(); i++) {
        layouts[i] = layout;
    }
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = pNext,
        .descriptorPool = pool,
        .descriptorSetCount = (uint32_t)layouts.size(),
        .pSetLayouts = layouts.data()
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));
};

inline void vkuCmdBindSingleVertexBuffer(VkCommandBuffer command_buffer, VkBuffer buffer) {
    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer, &offset);
}

inline void vkuCmdBindVertexBuffers(VkCommandBuffer command_buffer, Span<VkBuffer> buffers) {
    Vector<VkDeviceSize> offsets(buffers.size(), 0);
    vkCmdBindVertexBuffers(command_buffer, 0, buffers.size(), buffers.data(), offsets.data());
}

inline void vkuBeginCommandBuffer(VkCommandBuffer command_buffer) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
}