#include "renderer.h"

#include <vector>
#include <fstream>
#include <set>

#include <SDL.h>
#include <SDL_vulkan.h>

#include "SDL_video.h"
#include "vk_utils.h"
#include "vulkan/vulkan_core.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#include "stb/stb_image.h"

#include "imgui.h"

#include "res.h"
#include "mesh.h"
#include "terrain.h"
#include "core/log.h"
#include "core/file.h"

#include "render/imgui_renderer.h"
#include "render/im3d_renderer.h"
#include "render/wireframe_renderer.h"

#define TRACY_ENABLE
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

const char* g_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
};

const char* g_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            log_trace("Vulkan validation layer: {}", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            log_info("Vulkan validation layer: {}", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            log_warn("Vulkan validation layer: {}", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            log_error("Vulkan validation layer: {}", pCallbackData->pMessage); break;
        default:
            break;
    }
    return VK_FALSE;
}

static void check_vk_result(VkResult err) {
    VK_CHECK(err);
}

SwapchainSettings SwapchainSupport::select_default(struct SDL_Window* window) {
    SwapchainSettings settings = {};
    // Select a surface format that we want
    settings.surface_format = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            settings.surface_format = format;
        }
    }

    // Select a surface present mode that we want
    settings.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end()) {
        settings.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    // Choose the extent of the swapchain
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        settings.extent = capabilities.currentExtent;
    }
    else {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        VkExtent2D actual_extent = {(uint32_t)width, (uint32_t)height};
        actual_extent.width = glm::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = glm::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        settings.extent = actual_extent;
    }
    return settings;
}

VkPipelineShaderStageCreateInfo Shader::stage_info() {
    switch (type) {
        case ShaderType::Vertex: return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = module,
            .pName = "main"
        };
        case ShaderType::Fragment: return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = module,
            .pName = "main"
        };
        default: {
            log_error("Unsupported shader type {}!", (int)type);
            return {};
        }
    }
}

void ImageCpuData::load_from_file(const std::string& filename, int nchannels) {
    data_channels = nchannels;
    auto buf = load_file_to_buffer(filename);
    pixels = stbi_load_from_memory(buf.data(), buf.size(), &width, &height, &channels, data_channels);
}

void ImageCpuData::load_from_memory(const uint8_t* data, size_t size, int nchannels) {
    pixels = stbi_load_from_memory(data, size, &width, &height, &channels, nchannels);
}

void ImageCpuData::cleanup() {
    if (pixels) {
        stbi_image_free(pixels);
    }
}

RenderInterface::RenderInterface(Renderer *renderer)
    : _renderer(renderer), _ecs(renderer->_ecs) {}

void Renderer::init() {
    ZoneScoped;

    _lighting = LightingBuffer {
        .dir = {
            .direction = {1.0f, -1.0f, 1.0f},
            .intensity = 1.0f,
            .color = {1.0f, 1.0f, 1.0f}
        },
        .point = {
            {
                .position = {-5.0f, 5.0f, -5.0f},
                .intensity = 1.0f,
                .color = {1.0f, 0.0f, 0.0f}
            },
            {
                .position = {5.0f, 5.0f, -5.0f},
                .intensity = 1.0f,
                .color = {0.0f, 1.0f, 0.0f}
            },
            {
                .position = {-5.0f, 5.0f, 5.0f},
                .intensity = 1.0f,
                .color = {0.0f, 0.0f, 1.0f}
            },
            {
                .position = {5.0f, 5.0f, 5.0f},
                .intensity = 1.0f,
                .color = {1.0f, 1.0f, 1.0f}
            }
        },
        .num_points = 4
    };

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _window_extent.width = w;
    _window_extent.height = h;

    create_instance();
    create_surface();
    create_device();
    create_allocator();
    create_swapchain();
    create_descriptor_set_layout();
    create_depth_resources();

    create_command_pool();
    create_uniform_buffers();
    create_storage_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    create_command_buffers();
    create_sync_objects();

    _graphics_queue_tracy_ctx.resize(MAX_FRAMES_IN_FLIGHT);
    _compute_queue_tracy_ctx.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _graphics_queue_tracy_ctx[i] = TracyVkContextCalibrated(_physical_device, _device, _graphics_queue, _command_buffers[i],
            vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
        _compute_queue_tracy_ctx[i] = TracyVkContextCalibrated(_physical_device, _device, _compute_queue, _command_buffers[i],
            vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
        std::string graphics_queue_name = fmt::format("Graphics Queue {}", i);
        std::string compute_queue_name = fmt::format("Compute Queue {}", i);
        TracyVkContextName(_graphics_queue_tracy_ctx[i], graphics_queue_name.data(), graphics_queue_name.size());
        TracyVkContextName(_compute_queue_tracy_ctx[i], compute_queue_name.data(), compute_queue_name.size());
    }

    _is_initialized = true;
}

Camera &Renderer::get_current_camera() {
    return _ecs->get_component<Camera>(_camera_object);
}

#ifndef NDEBUG
#define VULKAN_USE_VALIATION_LAYER
#endif

void Renderer::begin_frame() {
    for (auto ri : _render_interfaces) {
        ri->begin_frame();
    }
}

void Renderer::end_frame() {
    for (auto ri : _render_interfaces) {
        ri->end_frame();
    }
}

void Renderer::cleanup() {
    ZoneScoped;

    if (_is_initialized) {
        auto res = Res::inst();

        res->_texture_pool.foreach([&](Texture& texture) {
            vkDestroySampler(_device, texture.sampler, nullptr);
            vkDestroyImageView(_device, texture.image_view, nullptr);
        });

        res->_textured_mesh_pool.foreach([&](TexturedMesh& mesh) {
            vmaDestroyBuffer(_vma_allocator, mesh.vertex_buffer.buffer, mesh.vertex_buffer.alloc_data);
            vmaDestroyBuffer(_vma_allocator, mesh.index_buffer.buffer, mesh.index_buffer.alloc_data);
        });

        res->_image_pool.foreach([&](Image& image) {
            vmaDestroyImage(_vma_allocator, image.image, image.alloc_data);
        });

        cleanup_swapchain();

        destroy_uniform_buffer(_uniform_buffer);

        vkDestroyImageView(_device, _depth_imageview, nullptr);
        vmaDestroyImage(_vma_allocator, _depth_image.image, _depth_image.alloc_data);

        vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(_device, _main_descriptor_set_layout, nullptr);

        vmaDestroyAllocator(_vma_allocator);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            TracyVkDestroy(_graphics_queue_tracy_ctx[i]);
            TracyVkDestroy(_compute_queue_tracy_ctx[i]);

            vkDestroySemaphore(_device, _image_available_semaphores[i], nullptr);
            vkDestroySemaphore(_device, _render_finished_semaphores[i], nullptr);
            vkDestroyFence(_device, _in_flight_fences[i], nullptr);
        }

        vkDestroyCommandPool(_device, _command_pool, nullptr);
        vkDestroyDevice(_device, nullptr);
#ifdef VULKAN_USE_VALIATION_LAYER
        vkDestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
#endif
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyInstance(_instance, nullptr);

        _is_initialized = false;
    }
}

void Renderer::render() {
    ZoneScoped;

    toposort_render_interfaces();

    vkWaitForFences(_device, 1, &_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _image_available_semaphores[_current_frame], VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    else if (result != VK_SUBOPTIMAL_KHR){
        VK_CHECK(result);
    }

    if (_uniform_buffer.is_dirty[_current_frame]) {
        update_uniform_buffer(_current_frame);
        _uniform_buffer.is_dirty[_current_frame] = false;
    }

    if (_texture_descriptor_set.is_dirty[_current_frame]) {
        update_texture_descriptor_sets(_current_frame);
        _texture_descriptor_set.is_dirty[_current_frame] = false;
    }

    if (_buffer_descriptor_set.is_dirty[_current_frame]) {
        update_material_buffer_descriptor_sets(_current_frame);
        _buffer_descriptor_set.is_dirty[_current_frame] = false;
    }

    if (_lighting_descriptor_set.is_dirty[_current_frame]) {
        update_lighting_buffer_descriptor_sets(_current_frame);
        _lighting_descriptor_set.is_dirty[_current_frame] = false;
    }

    vkResetFences(_device, 1, &_in_flight_fences[_current_frame]);

    vkResetCommandBuffer(_command_buffers[_current_frame], 0);
    record_command_buffer(_command_buffers[_current_frame], image_index);

    VkSemaphore wait_semaphores[] = {_image_available_semaphores[_current_frame]};
    VkSemaphore signal_semaphores[] = {_render_finished_semaphores[_current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = wait_semaphores,
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &_command_buffers[_current_frame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signal_semaphores,
    };
    VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit_info, _in_flight_fences[_current_frame]));

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &_swapchain,
        .pImageIndices = &image_index,
        .pResults = nullptr
    };
    result = vkQueuePresentKHR(_present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebuffer_resized) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _uniform_buffer.is_dirty[i] = true;
        }
        _framebuffer_resized = false;
        recreate_swapchain();
    }
    else{
        VK_CHECK(result);
    }

    FrameMark;

    _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::respond_to_window_event(SDL_Event* e) {
    if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
        int w, h;
        SDL_GetWindowSize(_window, &w, &h);
        _window_extent.width = w;
        _window_extent.height = h;

        log_info("window changed: {} x {}", w, h);

        _framebuffer_resized = true;
    }
    if (e->window.event == SDL_WINDOWEVENT_MINIMIZED) {
        _framebuffer_minimized = true;
    }
}

void Renderer::wait_until_device_idle() {
    vkDeviceWaitIdle(_device);
}

void Renderer::create_instance() {

    VK_CHECK(volkInitialize());

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "flock3d",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };

    // Get available extensions
    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    Vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

    /*
    log_info("Available extensions: ");
    for (const auto& ext : extensions) {
        log_info("\t{} (version {})", ext.extensionName, ext.specVersion);
    }
    */

    // Check validation layer support
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

#ifdef VULKAN_USE_VALIATION_LAYER
    bool is_validation_layer_valid = true;
    for (const char* layer_name : g_validation_layers) {
        auto it = std::find_if(available_layers.begin(), available_layers.end(), [=](VkLayerProperties& prop) {
            return strcmp(prop.layerName, layer_name) == 0;
        });
        if (it == available_layers.end()) {
            is_validation_layer_valid = false;
        }
    }
    if (!is_validation_layer_valid) {
        log_error("Vulkan validation layers requested but not available!");
        std::abort(); } create_info.enabledLayerCount = (uint32_t)(sizeof(g_validation_layers) / sizeof(g_validation_layers[0])); create_info.ppEnabledLayerNames = g_validation_layers;
#else
    create_info.enabledLayerCount = 0;
#endif

    // Get extensions required by SDL
    uint32_t sdl_extension_count = 0;
    SDL_Vulkan_GetInstanceExtensions(_window, &sdl_extension_count, nullptr);
    Vector<const char*> sdl_extensions(sdl_extension_count);
    SDL_Vulkan_GetInstanceExtensions(_window, &sdl_extension_count, sdl_extensions.data());
#ifdef VULKAN_USE_VALIATION_LAYER
    sdl_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    create_info.enabledExtensionCount = (uint32_t)sdl_extensions.size();
    create_info.ppEnabledExtensionNames = sdl_extensions.data();

#ifdef VULKAN_USE_VALIATION_LAYER
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
            .pUserData = nullptr,
    };
    create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_messenger_create_info;
#else
    create_info.pNext = nullptr;
#endif

    // Create instance
    VK_CHECK(vkCreateInstance(&create_info, nullptr, &_instance));
    volkLoadInstance(_instance);

    // Setup debug messenger
#ifdef VULKAN_USE_VALIATION_LAYER
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(_instance, &debug_messenger_create_info, nullptr, &_debug_messenger))
#endif
}

void Renderer::create_surface() {
    if (!SDL_Vulkan_CreateSurface(_window, _instance, &_surface)) {
        log_error("Failed to create Vulkan surface from SDL!");
        std::abort();
    }
}

void Renderer::create_device() {
    _physical_device = VK_NULL_HANDLE;
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
    if (device_count == 0) {
        log_error("Failed to find GPUs with Vulkan support!");
        std::abort();
    }
    Vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(_instance, &device_count, devices.data());

    _queue_family_main_idx = -1;
    for (const auto& device : devices) {
        // Filter out devices that are not "real" GPUs
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(device, &device_features);

        VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
        VkPhysicalDeviceFeatures2 device_features2 = { 
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, 
            .pNext = &indexing_features };
        vkGetPhysicalDeviceFeatures2(device, &device_features2);
        bool bindless_supported = indexing_features.descriptorBindingPartiallyBound && indexing_features.runtimeDescriptorArray;

        bool has_conventional_gpu = device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
                                    // || device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        // Need to filter out GPU without our needed features
        if (!has_conventional_gpu || 
            !device_features.samplerAnisotropy || 
            !device_features.wideLines || 
            !device_features.fillModeNonSolid ||
            !bindless_supported) {
            continue;
        }

        // Filter out devices that do not have swapchain support
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        Vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
        uint32_t device_extensions_count = sizeof(g_device_extensions) / sizeof(g_device_extensions[0]);
        bool missing_required_device_extension = false;
        for (int i = 0; i < device_extensions_count; i++) {
            const char* device_ext = g_device_extensions[i];
            auto it = std::find_if(available_extensions.begin(), available_extensions.end(), [=](VkExtensionProperties& ext) {
                return strcmp(ext.extensionName, device_ext) == 0;
            });
            if (it == available_extensions.end()) {
                missing_required_device_extension = true;
                break;
            }
        }
        if (missing_required_device_extension) continue;

        // Check if we can create an adequate swapchain on this device
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &_swapchain_support.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &format_count, nullptr);
        _swapchain_support.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &format_count, _swapchain_support.formats.data());

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &present_mode_count, nullptr);
        _swapchain_support.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &present_mode_count, _swapchain_support.present_modes.data());

        if (_swapchain_support.formats.empty() || _swapchain_support.present_modes.empty()) continue;

        // Only select devices that have graphics + present + compute queue
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        Vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        for (int queue_family_idx = 0; queue_family_idx < queue_family_count; queue_family_idx++) {
            const auto& queue_family = queue_families[queue_family_idx];
            VkBool32 present_support;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_idx, _surface, &present_support);
            uint32_t support_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
            if ((queue_family.queueFlags & support_flags) == support_flags && present_support) {
                _queue_family_main_idx = queue_family_idx;
                break;
            }
        }

        // Found our physical device!
        if (_queue_family_main_idx != -1) {
            _physical_device = device;
            break;
        }
    }
    if (_physical_device == VK_NULL_HANDLE) {
        log_error("Failed to find a suitable GPU!");
        std::abort();
    }

    vkGetPhysicalDeviceProperties(_physical_device, &_physical_device_properties);

    Array<float, 1> queue_priorities = {1.0f};
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = (uint32_t)_queue_family_main_idx,
        .queueCount = queue_priorities.size(),
        .pQueuePriorities = queue_priorities.data()
    };

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
        .pNext = &indexing_features
    };

    VkPhysicalDeviceFeatures2 physical_features2 = { 
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = {
            .samplerAnisotropy = VK_TRUE,
            .wideLines = VK_TRUE,
            .fillModeNonSolid = VK_TRUE,
        },
        .pNext = &dynamic_rendering_features,
    };

    /*
    VkPhysicalDeviceSynchronization2Features synchronization2_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &dynamic_rendering_feature,
        .synchronization2 = VK_TRUE
    };
    */
 
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physical_features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
    };

    create_info.enabledExtensionCount = 0;
#ifdef VULKAN_USE_VALIATION_LAYER
    create_info.enabledLayerCount = sizeof(g_validation_layers) / sizeof(g_validation_layers[0]);
    create_info.ppEnabledLayerNames = g_validation_layers;
#else
    create_info.enabledLayerCount = 0;
#endif
    create_info.enabledExtensionCount = sizeof(g_device_extensions) / sizeof(g_device_extensions[0]);
    create_info.ppEnabledExtensionNames = g_device_extensions;
    VK_CHECK(vkCreateDevice(_physical_device, &create_info, nullptr, &_device))

    vkGetDeviceQueue(_device, _queue_family_main_idx, 0, &_graphics_queue);
    vkGetDeviceQueue(_device, _queue_family_main_idx, 0, &_compute_queue);
    vkGetDeviceQueue(_device, _queue_family_main_idx, 0, &_present_queue);
}

void Renderer::create_swapchain() {
    _swapchain_settings = _swapchain_support.select_default(_window);

    uint32_t image_count = _swapchain_support.capabilities.minImageCount + 1;
    if (_swapchain_support.capabilities.maxImageCount > 0 && image_count > _swapchain_support.capabilities.maxImageCount) {
        image_count = _swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = _surface,
            .minImageCount = image_count,
            .imageFormat = _swapchain_settings.surface_format.format,
            .imageColorSpace = _swapchain_settings.surface_format.colorSpace,
            .imageExtent = _swapchain_settings.extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = _swapchain_support.capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = _swapchain_settings.present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain));

    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr);
    _swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data());

    _swapchain_imageviews.resize(image_count);
    for (uint32_t i = 0; i < _swapchain_images.size(); i++) {
        _swapchain_imageviews[i] = vkuCreateImageView(_device, _swapchain_images[i], _swapchain_settings.surface_format.format);
    }
}

void Renderer::create_command_pool() {
    _command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = (uint32_t) _queue_family_main_idx 
    };
    VK_CHECK(vkCreateCommandPool(_device, &pool_info, nullptr, &_command_pool));
}

void Renderer::create_depth_resources() {
    // TODO: We just assume we have D32_SFLOAT support in our GPU for now.
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = {.width = _swapchain_settings.extent.width, .height = _swapchain_settings.extent.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .samples = VK_SAMPLE_COUNT_1_BIT,
    };
    VmaAllocationCreateInfo image_alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .priority = 1.0f
    };
    VmaAllocationInfo image_alloc_info;
    VK_CHECK(vmaCreateImage(_vma_allocator, &image_info, &image_alloc_create_info,
                            &_depth_image.image, &_depth_image.alloc_data, &image_alloc_info));
    _depth_imageview = vkuCreateImageView(_device, _depth_image.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::create_allocator() {
    VmaVulkanFunctions vulkan_functions = {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory = vkAllocateMemory,
            .vkFreeMemory = vkFreeMemory,
            .vkMapMemory = vkMapMemory,
            .vkUnmapMemory = vkUnmapMemory,
            .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory = vkBindBufferMemory,
            .vkBindImageMemory = vkBindImageMemory,
            .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
            .vkCreateBuffer = vkCreateBuffer,
            .vkDestroyBuffer = vkDestroyBuffer,
            .vkCreateImage = vkCreateImage,
            .vkDestroyImage = vkDestroyImage,
            .vkCmdCopyBuffer = vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR = vkBindBufferMemory2,
            .vkBindImageMemory2KHR = vkBindImageMemory2,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
            .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
            .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
    };

    VmaAllocatorCreateInfo allocator_create_info = {
            .physicalDevice = _physical_device,
            .device = _device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = _instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    VK_CHECK(vmaCreateAllocator(&allocator_create_info, &_vma_allocator));
}

void Renderer::create_descriptor_set_layout() {

    Array<VkDescriptorSetLayoutBinding, 1> main_bindings = {
        VkDescriptorSetLayoutBinding {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    vkuCreateDescriptorSetLayout(_device, main_bindings, &_main_descriptor_set_layout);

    VkDescriptorBindingFlags bindless_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindless_flag,
    };

    Array<VkDescriptorSetLayoutBinding, 1> texture_bindings = {
        VkDescriptorSetLayoutBinding {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_BINDLESS_TEXTURES,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    vkuCreateDescriptorSetLayout(_device, texture_bindings, &_texture_descriptor_set_layout, &flag_info);

    Array<VkDescriptorSetLayoutBinding, 1> buffer_bindings = {
        VkDescriptorSetLayoutBinding {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = MAX_BINDLESS_BUFFERS,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    vkuCreateDescriptorSetLayout(_device, buffer_bindings, &_buffer_descriptor_set_layout, &flag_info);

    Array<VkDescriptorSetLayoutBinding, 1> lighting_bindings = {
        VkDescriptorSetLayoutBinding {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    vkuCreateDescriptorSetLayout(_device, lighting_bindings, &_lighting_descriptor_set_layout, nullptr);
}

void Renderer::create_descriptor_sets() {

    vkuCreateDescriptorSets(_device, _descriptor_pool, _main_descriptor_set_layout, _main_descriptor_set.set_per_frame);
    Vector<VkWriteDescriptorSet> writes;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = _uniform_buffer.buffer_per_frame[i].buffer,
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };
        writes.push_back(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
            .dstSet = _main_descriptor_set.set_per_frame[i],
            .dstBinding = 0,
        });
    }
    vkUpdateDescriptorSets(_device, writes.size(), writes.data(), 0, nullptr);

    Array<uint32_t, MAX_FRAMES_IN_FLIGHT> max_texture_binding;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        max_texture_binding[i] = MAX_BINDLESS_TEXTURES - 1;
    }
    VkDescriptorSetVariableDescriptorCountAllocateInfo texture_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pDescriptorCounts = max_texture_binding.data(),
    };
    vkuCreateDescriptorSets(_device, _descriptor_pool, _texture_descriptor_set_layout, 
        _texture_descriptor_set.set_per_frame, &texture_count_info);

    Array<uint32_t, MAX_FRAMES_IN_FLIGHT> max_buffer_binding;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        max_buffer_binding[i] = MAX_BINDLESS_BUFFERS - 1;
    }
    VkDescriptorSetVariableDescriptorCountAllocateInfo buffer_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pDescriptorCounts = max_buffer_binding.data(),
    };
    vkuCreateDescriptorSets(_device, _descriptor_pool, _buffer_descriptor_set_layout, 
        _buffer_descriptor_set.set_per_frame, &buffer_count_info);

    vkuCreateDescriptorSets(_device, _descriptor_pool, _lighting_descriptor_set_layout, 
        _lighting_descriptor_set.set_per_frame, nullptr);
}

void Renderer::create_uniform_buffers() {
    _uniform_buffer = create_uniform_buffer(sizeof(UniformBufferObject));
}

void Renderer::create_storage_buffers() {
    _material_buffer = create_storage_buffer(sizeof(MaterialGpu) * MAX_MATERIALS);
    _lighting_buffer = create_storage_buffer(sizeof(LightingBuffer));
}

void Renderer::create_descriptor_pool() {
    Array<VkDescriptorPoolSize, 4> pool_sizes = {
        VkDescriptorPoolSize {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        },
        VkDescriptorPoolSize {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT * MAX_BINDLESS_TEXTURES,
        },
        VkDescriptorPoolSize {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT * MAX_BINDLESS_IMAGES,
        },
        VkDescriptorPoolSize {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT * (MAX_BINDLESS_BUFFERS + 1),
        },

    };
    VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = pool_sizes.size(),
            .pPoolSizes = pool_sizes.data(),
            .maxSets = 4 * MAX_FRAMES_IN_FLIGHT
    };
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptor_pool));
}

void Renderer::create_command_buffers() {
    VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = _command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = (uint32_t) _command_buffers.size()
    };
    VK_CHECK(vkAllocateCommandBuffers(_device, &alloc_info, _command_buffers.data()));
}

void Renderer::create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    _image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr, &_image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr, &_render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence(_device, &fence_info, nullptr, &_in_flight_fences[i]));
    }
}

void Renderer::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index) {
    ZoneScoped;

    vkuBeginCommandBuffer(command_buffer);

    VkImageMemoryBarrier begin_color_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = _swapchain_images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        // .srcQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
        // .dstQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
    };

    VkImageMemoryBarrier begin_depth_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = _depth_image.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        // .srcQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
        // .dstQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1, &begin_color_image_memory_barrier
    );

    /*
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &begin_depth_image_memory_barrier
    );
*/

    const auto& camera = get_current_camera();

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = _swapchain_imageviews[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {0.0f, 0.0f, 0.0f, 1.0f}}
    };
    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = _depth_imageview,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = {1.0f, 0}}
    };

    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, _swapchain_settings.extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };

    {
        TracyVkZone(_graphics_queue_tracy_ctx[_current_frame], command_buffer, "MainRenderPass")

        vkCmdBeginRendering(command_buffer, &render_info);

        for (auto* rp : _render_interfaces) {
            rp->render(command_buffer);
        }

        vkCmdEndRendering(command_buffer);
    }

    VkImageMemoryBarrier end_color_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = _swapchain_images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        // .srcQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
        // .dstQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
    };

    VkImageMemoryBarrier end_depth_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = _depth_image.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        // .srcQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
        // .dstQueueFamilyIndex = (uint32_t)_queue_family_main_idx,
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1, &end_color_image_memory_barrier
    );

    /*
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1, &end_depth_image_memory_barrier
    );
*/

    TracyVkCollect(_graphics_queue_tracy_ctx[_current_frame], command_buffer);
    TracyVkCollect(_compute_queue_tracy_ctx[_current_frame], command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

Buffer Renderer::create_static_render_buffer_from_cpu(VkBufferUsageFlags buffer_usage, const void* data, size_t size) {
    Buffer rb = {};

    // Create staging buffer and copy CPU data into it
    VkBuffer staging_buffer;
    VkBufferCreateInfo staging_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo staging_buffer_alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocation staging_buffer_alloc;
    VmaAllocationInfo staging_buffer_alloc_info;
    VK_CHECK(vmaCreateBuffer(_vma_allocator, &staging_buffer_info, &staging_buffer_alloc_create_info,
                             &staging_buffer, &staging_buffer_alloc, &staging_buffer_alloc_info));
    memcpy(staging_buffer_alloc_info.pMappedData, data, size);

    // Create vertex buffer and copy from staging buffer
    VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usage,
    };
    VmaAllocationCreateInfo buffer_alloc_create_info = {
            .flags = (VmaAllocationCreateFlags)VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f
    };
    VK_CHECK(vmaCreateBuffer(_vma_allocator, &buffer_info, &buffer_alloc_create_info,
                             &rb.buffer, &rb.alloc_data, nullptr));
    rb.size = size;

    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferCopy copy_region = {.size = size};
    vkCmdCopyBuffer(command_buffer, staging_buffer, rb.buffer, 1, &copy_region);

    end_single_time_commands(command_buffer);

    vmaDestroyBuffer(_vma_allocator, staging_buffer, staging_buffer_alloc);

    return rb;
}

void Renderer::create_static_render_buffer_from_cpu(VkCommandBuffer cmd_buffer,
    VkBufferUsageFlags buffer_usage, const void *data, size_t size,
    Buffer& staging_buffer, Buffer& dest_buffer) {

    // Create staging buffer and copy CPU data into it
    VkBufferCreateInfo staging_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo staging_buffer_alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo staging_buffer_alloc_info;
    VK_CHECK(vmaCreateBuffer(_vma_allocator, &staging_buffer_info, &staging_buffer_alloc_create_info,
                             &staging_buffer.buffer, &staging_buffer.alloc_data, &staging_buffer_alloc_info));
    staging_buffer.size = size;
    memcpy(staging_buffer_alloc_info.pMappedData, data, size);

    // Create vertex buffer and copy from staging buffer
    VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usage,
    };
    VmaAllocationCreateInfo buffer_alloc_create_info = {
            .flags = (VmaAllocationCreateFlags)VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f
    };
    VK_CHECK(vmaCreateBuffer(_vma_allocator, &buffer_info, &buffer_alloc_create_info,
                             &dest_buffer.buffer, &dest_buffer.alloc_data, nullptr));
    dest_buffer.size = size;

    VkBufferCopy copy_region = {.size = size};
    vkCmdCopyBuffer(cmd_buffer, staging_buffer.buffer, dest_buffer.buffer, 1, &copy_region);
}

Buffer Renderer::create_dynamic_render_buffer(VkBufferUsageFlags buffer_usage, size_t initial_size) {
    Buffer buf = {};

    VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = initial_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usage
    };

    VmaAllocationCreateInfo alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f
    };

    VmaAllocationInfo alloc_info;
    vmaCreateBuffer(_vma_allocator, &buffer_info, &alloc_create_info,
                    &buf.buffer, &buf.alloc_data, &alloc_info);
    buf.size = initial_size;

    return buf;
}

void Renderer::create_or_resize_dynamic_buffer(VkBufferUsageFlags buffer_usage, uint32_t cur_frame, size_t new_size, DynamicBuffer& db) {
    auto& buf = db.buffer_per_frame[cur_frame];
    if (buf.buffer == VK_NULL_HANDLE) {
        buf = create_dynamic_render_buffer(buffer_usage, new_size);
    }
    else {
        if (buf.size < new_size) {
            destroy_buffer(buf);
            buf = create_dynamic_render_buffer(buffer_usage, new_size);
        }
    }
}

Buffer Renderer::create_vertex_buffer(const void *data, size_t size) {
    return create_static_render_buffer_from_cpu(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, data, size);
}

Buffer Renderer::create_index_buffer(const void *data, size_t size) {
    return create_static_render_buffer_from_cpu(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, data, size);
}

UniformBuffer Renderer::create_uniform_buffer(size_t size) {
    UniformBuffer ub;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        VmaAllocationCreateInfo alloc_create_info = {
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .priority = 1.0f
        };

        VmaAllocationInfo alloc_info;
        vmaCreateBuffer(_vma_allocator, &buffer_info, &alloc_create_info,
                        &ub.buffer_per_frame[i].buffer, &ub.buffer_per_frame[i].alloc_data, &alloc_info);
        ub.buffer_per_frame[i].size = size;
        ub.is_dirty[i] = true;
    }
    ub.size = size;
    return ub;
}

StorageBuffer Renderer::create_storage_buffer(size_t size) {
    StorageBuffer sb;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        VmaAllocationCreateInfo alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f
        };

        VmaAllocationInfo alloc_info;
        vmaCreateBuffer(_vma_allocator, &buffer_info, &alloc_create_info,
            &sb.buffer_per_frame[i].buffer, &sb.buffer_per_frame[i].alloc_data, &alloc_info);
        sb.buffer_per_frame[i].size = size;
        sb.is_dirty[i] = true;
    }
    sb.size = size;
    return sb;
}

void* Renderer::get_mapped_pointer(const Buffer& buffer) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(_vma_allocator, buffer.alloc_data, &alloc_info);
    return alloc_info.pMappedData;
}

void* Renderer::get_mapped_pointer(const DynamicBuffer& dynamic_buffer, uint32_t cur_frame) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(_vma_allocator, dynamic_buffer.buffer_per_frame[cur_frame].alloc_data, &alloc_info);
    return alloc_info.pMappedData;
}

void* Renderer::get_mapped_pointer(const UniformBuffer& uniform_buffer, uint32_t cur_frame) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(_vma_allocator, uniform_buffer.buffer_per_frame[cur_frame].alloc_data, &alloc_info);
    return alloc_info.pMappedData;
}

void Renderer::destroy_buffer(const Buffer &buffer) {
    vmaDestroyBuffer(_vma_allocator, buffer.buffer, buffer.alloc_data);
}

void Renderer::destroy_dynamic_buffer(const DynamicBuffer& dynamic_buffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(_vma_allocator, dynamic_buffer.buffer_per_frame[i].buffer, dynamic_buffer.buffer_per_frame[i].alloc_data);
    }
}
void Renderer::destroy_uniform_buffer(const UniformBuffer &uniform_buffer) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(_vma_allocator, uniform_buffer.buffer_per_frame[i].buffer, uniform_buffer.buffer_per_frame[i].alloc_data);
    }
}

void Renderer::cleanup_swapchain() {
    for (uint32_t i = 0; i < _swapchain_imageviews.size(); i++) {
        vkDestroyImageView(_device, _swapchain_imageviews[i], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::recreate_swapchain() {
    if (_framebuffer_minimized) {
        SDL_Event e;
        do {
            SDL_WaitEvent(&e);
        } while (e.window.event == SDL_WINDOWEVENT_RESTORED);

        _framebuffer_minimized = false;
    }

    vkDeviceWaitIdle(_device);

    cleanup_swapchain();

    create_swapchain();
    create_depth_resources();
}

uint32_t Renderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(_physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    log_error("Failed to find suitable memory type!");
    return 0; // unreachable
}

void Renderer::update_uniform_buffer(uint32_t cur_frame, const UniformBuffer& buffer, void* data, size_t size) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(_vma_allocator, buffer.buffer_per_frame[cur_frame].alloc_data, &alloc_info);
    memcpy(alloc_info.pMappedData, data, size);
}

void Renderer::update_storage_buffer(uint32_t cur_frame, const DescriptorSet& descriptor_set, const StorageBuffer& buffer, void* data, size_t size) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(_vma_allocator, buffer.buffer_per_frame[cur_frame].alloc_data, &alloc_info);
    memcpy(alloc_info.pMappedData, data, size);

    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffer.buffer_per_frame[cur_frame].buffer,
        .offset = 0,
        .range = size
    };

    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstSet = descriptor_set.set_per_frame[cur_frame],
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info
    };

    vkUpdateDescriptorSets(_device, 1, &descriptor_write, 0, nullptr);
}

void Renderer::update_uniform_buffer(uint32_t cur_image) {
    UniformBufferObject ubo = {};
    ubo.proj = get_current_camera().proj_mat;
    ubo.viewport_size = {(float)_window_extent.width, (float)_window_extent.height};

    update_uniform_buffer(cur_image, _uniform_buffer, &ubo, sizeof(UniformBufferObject));
}

void Renderer::update_texture_descriptor_sets(uint32_t cur_image) {
    auto res = Res::inst();

    if (res->_texture_pool.size() == 0) return;

    Array<VkWriteDescriptorSet, MAX_BINDLESS_TEXTURES> bindless_descriptor_writes;
    Array<VkDescriptorImageInfo, MAX_BINDLESS_TEXTURES>  bindless_image_info;

    uint32_t current_write_index = 0;
    res->_texture_pool.foreach_with_ref([&](Ref<Texture> tex_id, Texture& tex) {
        bindless_image_info[current_write_index] = VkDescriptorImageInfo {
            .sampler = tex.sampler,
            .imageView = tex.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        bindless_descriptor_writes[current_write_index] = VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .descriptorCount = 1,
            .dstArrayElement = current_write_index,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .dstSet = _texture_descriptor_set.set_per_frame[cur_image],
            .dstBinding = 0,
            .pImageInfo = &bindless_image_info[current_write_index],
        };

        current_write_index++;
    });

    vkUpdateDescriptorSets(_device, current_write_index, bindless_descriptor_writes.data(), 0, nullptr);
}

void Renderer::update_material_buffer_descriptor_sets(uint32_t cur_image) {
    auto res = Res::inst();

    Material* material_buf = res->_material_pool.item_buf();
    const uint32_t material_count = res->_material_pool.size();
    if (material_count == 0) return;

    MaterialGpu* material_gpu_buf = new MaterialGpu[material_count];
    for (uint32_t i = 0; i < material_count; i++) {
        material_gpu_buf[i].albedo = material_buf[i].albedo;
        material_gpu_buf[i].metallic = material_buf[i].metallic;
        material_gpu_buf[i].roughness = material_buf[i].roughness;
        material_gpu_buf[i].ao = material_buf[i].ao;
        material_gpu_buf[i].albedo_tex_id = res->_texture_pool.get_item_idx(material_buf[i].albedo_tex_id);
        material_gpu_buf[i].metallic_roughness_tex_id = res->_texture_pool.get_item_idx(material_buf[i].metallic_roughness_tex_id);
        material_gpu_buf[i].ao_tex_id = res->_texture_pool.get_item_idx(material_buf[i].ao_tex_id);
    }

    update_storage_buffer(cur_image, _buffer_descriptor_set, _material_buffer, material_gpu_buf, sizeof(MaterialGpu) * material_count);

    delete [] material_gpu_buf;
}

void Renderer::update_lighting_buffer_descriptor_sets(uint32_t cur_image) {
    update_storage_buffer(cur_image, _lighting_descriptor_set, _lighting_buffer, &_lighting, sizeof(LightingBuffer));
}

VkCommandBuffer Renderer::begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandPool = _command_pool,
            .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(_device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void Renderer::end_single_time_commands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer
    };

    vkQueueSubmit(_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphics_queue);

    vkFreeCommandBuffers(_device, _command_pool, 1, &command_buffer);
}

void Renderer::copy_buffer(VkCommandBuffer cmd_buffer, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
    VkBufferCopy copy_region = {size};
    vkCmdCopyBuffer(cmd_buffer, src_buffer, dst_buffer, 1, &copy_region);
}

void
Renderer::transition_image_layout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = VkImageSubresourceRange {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            },
    };

    // TODO: we'll probably simplify this later using https://github.com/Tobski/simple_vulkan_synchronization
    VkPipelineStageFlags src_stage, dst_stage;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        log_error("Unsupported layout transition!");
        std::abort();
    };

    vkCmdPipelineBarrier(cmd_buffer,
                         src_stage, dst_stage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

void Renderer::copy_buffer_to_image(VkCommandBuffer cmd_buffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(cmd_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Renderer::upload_to_gpu(const ImageCpuData &image_cpu, VkFormat format, Image& image) {
    VkCommandBuffer cmd_buffer = begin_single_time_commands();
    Buffer staging_buffer;
    upload_to_gpu(cmd_buffer, image_cpu, format, image, staging_buffer);
    end_single_time_commands(cmd_buffer);
    destroy_buffer(staging_buffer);
}

void Renderer::upload_to_gpu(VkCommandBuffer cmd_buffer, const ImageCpuData& image_cpu, VkFormat format, Image& image, Buffer& staging_buffer) {
    VkDeviceSize image_size = image_cpu.width * image_cpu.height * image_cpu.data_channels;

    VkBufferCreateInfo staging_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = image_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo staging_buffer_alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo staging_buffer_alloc_info;
    VK_CHECK(vmaCreateBuffer(_vma_allocator, &staging_buffer_info, &staging_buffer_alloc_create_info,
                             &staging_buffer.buffer, &staging_buffer.alloc_data, &staging_buffer_alloc_info));
    memcpy(staging_buffer_alloc_info.pMappedData, image_cpu.pixels, image_size);

    image = {.extents = {(uint32_t)image_cpu.width, (uint32_t)image_cpu.height}, .format = format};

    VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = {.width = (uint32_t)image_cpu.width, .height = (uint32_t)image_cpu.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .samples = VK_SAMPLE_COUNT_1_BIT,
    };
    VmaAllocationCreateInfo image_alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f
    };
    VmaAllocationInfo image_alloc_info;
    VK_CHECK(vmaCreateImage(_vma_allocator, &image_info, &image_alloc_create_info,
                            &image.image, &image.alloc_data, &image_alloc_info));

    transition_image_layout(cmd_buffer, image.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(cmd_buffer, staging_buffer.buffer, image.image, image_cpu.width, image_cpu.height);
    transition_image_layout(cmd_buffer, image.image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

Shader Renderer::load_shader_from_file(const std::string& filename, ShaderType type) {
    Shader shader;
    auto shader_code = load_file_to_buffer(filename);
    if (shader_code.empty()) return shader;
    VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = shader_code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    VK_CHECK(vkCreateShaderModule(_device, &create_info, nullptr, &shader.module));
    shader.type = type;
    return shader;
}

void Renderer::destroy_shader(const Shader &shader) {
    vkDestroyShaderModule(_device, shader.module, nullptr);
}

Ref<Image> Renderer::load_image_from_file(const std::string &filename, VkFormat format) {
    auto res = Res::inst();
    auto [image_id, image] = res->_image_pool.emplace();
    ImageCpuData image_cpu;
    image_cpu.load_from_file(filename);
    upload_to_gpu(image_cpu, format, *image);
    return image_id;
}

Ref<Texture> Renderer::create_texture(const Image& image, VkFormat format) {
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = _physical_device_properties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f
    };
    return create_texture(image, format, sampler_info);
}

Ref<Texture> Renderer::create_texture(const Image& image, VkFormat format, VkSamplerCreateInfo sampler_info) {
    auto res = Res::inst();
    auto [tex_id, tex] = res->_texture_pool.emplace();
    tex->image_view = vkuCreateImageView(_device, image.image, format);
    VK_CHECK(vkCreateSampler(_device, &sampler_info, nullptr, &tex->sampler));
    return tex_id;
}

VkViewport Renderer::default_viewport() {
    return VkViewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) _swapchain_settings.extent.width,
            .height = (float) _swapchain_settings.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
    };
}

VkRect2D Renderer::default_scissor() {
    return VkRect2D {
            .offset = {0, 0},
            .extent = _swapchain_settings.extent
    };
}

void Renderer::_toposort_visit(RenderInterface* ri, Vector<RenderInterface*>& sorted) {
    if (ri->_marked) return;

    for (auto dep : ri->_deps) {
        _toposort_visit(dep, sorted);
    }

    ri->_marked = true;
    sorted.push_back(ri);
};

void Renderer::toposort_render_interfaces() {
    Set<RenderInterface*> ri_set(_render_interfaces.begin(), _render_interfaces.end());
    for (auto ri : _render_interfaces) {
        for (auto dep : ri->_deps) {
            if (ri_set.find(dep) == ri_set.end()) {
                log_warn("Dependency not added to render interface list! Removing it from dependency...");
                ri->_deps.erase(dep);
            }
        }
        ri->_marked = false;
    }

    Vector<RenderInterface*> sorted;

    for (auto ri : _render_interfaces) {
        _toposort_visit(ri, sorted);
    }

    _render_interfaces = sorted;
}

void Renderer::render_imgui() {
    auto res = Res::inst();
    if (ImGui::CollapsingHeader("Lighting")) {
        auto& lighting = get_lighting_data();
        bool lighting_dirty = false;
        if (ImGui::TreeNode("Directional Light")) {
            lighting_dirty |= ImGui::DragFloat3("Direction", (float*)&lighting.dir.direction, 0.01f);
            lighting_dirty |= ImGui::ColorEdit3("Color", (float*)&lighting.dir.color);
            lighting_dirty |= ImGui::DragFloat("Intensity", &lighting.dir.intensity, 0.1f, 0.0f, 100.0f);
            ImGui::TreePop();
        }
        for (int i = 0; i < lighting.num_points; i++) {
            std::string light_name = "Point Light ";
            light_name += std::to_string(i);
            if (ImGui::TreeNode(light_name.c_str())) {
                lighting_dirty |= ImGui::DragFloat3("Position", (float*)&lighting.point[i].position);
                lighting_dirty |= ImGui::ColorEdit3("Color", (float*)&lighting.point[i].color);
                lighting_dirty |= ImGui::DragFloat("Intensity", &lighting.point[i].intensity, 0.1f, 0.0f, 100.0f);
                ImGui::TreePop();
            }
        }
        if (lighting_dirty) {
            lighting.dir.direction = glm::normalize(lighting.dir.direction);
            set_lighting_dirty();
        }
    }
    if (ImGui::CollapsingHeader("Material")) {
        bool material_dirty = false;
        res->_material_pool.foreach_with_ref([&](Ref<Material> mat_id, Material& mat) {
            const int img_size = 200;
            uint32_t idx = mat_id.index;
            uint32_t gen = mat_id.generation;
            std::string material_name = fmt::format("Material (idx={}, gen={})", idx, gen);
            if (ImGui::TreeNode(material_name.c_str())) {
                material_dirty |= ImGui::ColorEdit4("Albedo", (float*)&mat.albedo);
                material_dirty |= ImGui::DragFloat("Metallic", &mat.metallic, 0.01f);
                material_dirty |= ImGui::DragFloat("Roughness", &mat.roughness, 0.01f);
                material_dirty |= ImGui::DragFloat("AO", &mat.ao, 0.01f);
                ImGui::Text("Albedo Texture");
                ImGui::Image(mat.albedo_tex_id.to_userpointer(), ImVec2(img_size, img_size));
                ImGui::Text("Metallic / Roughness Texture");
                ImGui::Image(mat.metallic_roughness_tex_id.to_userpointer(), ImVec2(img_size, img_size));
                ImGui::Text("AO Texture");
                ImGui::Image(mat.ao_tex_id.to_userpointer(), ImVec2(img_size, img_size));
                ImGui::TreePop();
            }
        });
    }
    if (ImGui::CollapsingHeader("Texture")) {
        res->_texture_pool.foreach_with_ref([&](Ref<Texture> tex_id, Texture& tex) {
            const int img_size = 200;
            uint32_t idx = tex_id.index;
            uint32_t gen = tex_id.generation;
            std::string texture_name = fmt::format("Texture (idx={}, gen={})", idx, gen);
            if (ImGui::TreeNode(texture_name.c_str())) {
                ImGui::Image(tex_id.to_userpointer(), ImVec2(img_size, img_size));
                ImGui::TreePop();
            }
        });
    }
}