#pragma once

#include <initializer_list>
#include <volk.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <parallel_hashmap/phmap_utils.h>

#include "core/set.h"
#include "core/array.h"
#include "core/vector.h"
#include "core/storage.h"

#include "vk_mem_alloc.h"
#include "SDL_events.h"

#include "ecs.h"
#include "vulkan/vulkan_core.h"

struct SDL_Window;

struct ImageCpuData;
struct TexturedMeshCpuData;

class MeshRenderer;
class ImGuiRenderer;
class Im3dRenderer;
class WireframeRenderer;

class ECS;

namespace tracy { class VkCtx; }

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr int MAX_BINDLESS_TEXTURES = 1024;
constexpr int MAX_BINDLESS_IMAGES = 1024;
constexpr int MAX_BINDLESS_BUFFERS = 1024;

constexpr int MAX_MATERIALS = 1024;

struct SwapchainSettings {
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    Vector<VkSurfaceFormatKHR> formats;
    Vector<VkPresentModeKHR> present_modes;

    SwapchainSettings select_default(struct SDL_Window* window);
};

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc_data = VK_NULL_HANDLE;
    size_t size = 0;
};

struct DynamicBuffer { 
    Array<Buffer, MAX_FRAMES_IN_FLIGHT> buffer_per_frame;
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkExtent2D extents = {0, 0};
    VkFormat format;
    VmaAllocation alloc_data;
};

struct Texture {
    VkImageView image_view;
    VkSampler sampler;
};

enum class ShaderType {
    Invalid, Vertex, Fragment, Geometry, Tessellation, Compute
};

struct Shader {
    VkShaderModule module = VK_NULL_HANDLE;
    ShaderType type = ShaderType::Invalid;

    VkPipelineShaderStageCreateInfo stage_info();
};

struct UniformBuffer {
    Array<Buffer, MAX_FRAMES_IN_FLIGHT> buffer_per_frame;
    Array<bool, MAX_FRAMES_IN_FLIGHT> is_dirty;
    uint32_t size;

    UniformBuffer() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            is_dirty[i] = true;
        }
    }
};

struct StorageBuffer {
    Array<Buffer, MAX_FRAMES_IN_FLIGHT> buffer_per_frame;
    Array<bool, MAX_FRAMES_IN_FLIGHT> is_dirty;
    uint32_t size;

    StorageBuffer() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            is_dirty[i] = true;
        }
    }
};

struct DescriptorSet {
    Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> set_per_frame;
    Array<bool, MAX_FRAMES_IN_FLIGHT> is_dirty;
    bool is_built;

    DescriptorSet() : is_built(false) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            is_dirty[i] = true;
        }
    }
};

struct ColoredVertex {
    glm::vec3 pos;
    glm::vec3 color;

    bool operator==(const ColoredVertex& other) const {
        return pos == other.pos && color == other.color;
    }

    friend size_t hash_value(const ColoredVertex& v) {
        return phmap::HashState().combine(0, v.pos[0], v.pos[1], v.pos[2], v.color[0], v.color[1], v.color[2]);
    }
};

struct TexturedVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texcoord;

    bool operator==(const TexturedVertex& other) const {
        return pos == other.pos && normal == other.normal && texcoord == other.texcoord;
    }

    friend size_t hash_value(const TexturedVertex& v) {
        return phmap::HashState().combine(0, 
            v.pos[0], v.pos[1], v.pos[2], 
            v.normal[0], v.normal[1], v.normal[2],
            v.texcoord[0], v.texcoord[1]);
    }
};

struct ImageCpuData {
    unsigned char* pixels = nullptr;
    int width = 0, height = 0, channels = 0, data_channels = 0;

    void load_from_file(const std::string& filename, int nchannels = 4);
    void load_from_memory(const uint8_t* data, size_t size, int nchannels = 4);

    void cleanup();
};

struct alignas(16) UniformBufferObject {
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec2 viewport_size;
};

struct alignas(16) MeshPushConstants {
    alignas(16) glm::mat4 view_model;
    alignas(16) glm::vec4 color;
    glm::vec3 cam_pos;
    uint32_t mat_id;
};

#define MAX_POINT_LIGHTS 256

struct alignas(16) Material {
    glm::vec4 albedo;
    float metallic;
    float roughness;
    float ao;

    Ref<Texture> albedo_tex_id;
    Ref<Texture> metallic_roughness_tex_id;
    Ref<Texture> ao_tex_id;
};

struct alignas(16) MaterialGpu {
    glm::vec4 albedo;
    float metallic;
    float roughness;
    float ao;
    float _padding;

    uint32_t albedo_tex_id;
    uint32_t metallic_roughness_tex_id;
    uint32_t ao_tex_id;
};

struct alignas(16) DirectionalLight {
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float _padding2;
};

struct alignas(16) PointLight {
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float _padding2;
};

struct alignas(16) LightingBuffer {
    DirectionalLight dir;
    PointLight point[MAX_POINT_LIGHTS];
    uint32_t num_points;
};

struct Mesh {
    Buffer vertex_buffer;
    Buffer index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;

    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
};

struct TexturedMesh : public Mesh {
    Ref<Material> mat_id;
    DescriptorSet descriptor_set;
};

class Renderer;

class RenderInterface {
public:
    friend class Renderer;

    RenderInterface(Renderer *renderer);

    virtual void begin_frame() {};
    virtual void render(VkCommandBuffer command_buffer) = 0;
    virtual void end_frame() {}

    void set_deps(std::initializer_list<RenderInterface*> deps) {
        _deps = deps;
        for (auto dep : deps) {
            if (dep == this) {
                log_warn("Cannot insert itself as dependency! Removing it.");
                _deps.erase(dep);
            }
        }
    }

    void add_deps(std::initializer_list<RenderInterface*> deps) {
        for (auto dep : deps) {
            if (dep == this) {
                log_warn("Cannot insert itself as dependency! Removing it.");
            }
            else {
                _deps.insert(dep);
            }
        }
    }

protected:
    Renderer* _renderer;
    ECS* _ecs;

    Set<RenderInterface*> _deps;

    // Used for toposort
    bool _marked = false;
};

class Renderer {
public:
    friend class RenderInterface;

    friend class ImGuiRenderer;
    friend class Im3dRenderer;
    friend class TerrainRenderer;

    Renderer(struct SDL_Window* window, ECS* ecs) : _window(window), _ecs(ecs) {}

    void add_render_interface(RenderInterface* main_render_pass) {
        _render_interfaces.push_back(main_render_pass);
    }

    void set_camera_object(Entity camera_object) {
        this->_camera_object = camera_object;
    }

    Camera& get_current_camera();

    LightingBuffer& get_lighting_data() { return _lighting; }
    void set_lighting_dirty() { 
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            _lighting_descriptor_set.is_dirty[i] = true;
        }
    }

    VkDevice get_device() { return _device; }
    const SwapchainSupport& get_swapchain_support() const { return _swapchain_support; }
    const SwapchainSettings& get_swapchain_settings() const { return _swapchain_settings; }
    const VkPhysicalDeviceProperties& get_physical_device_properties() const { return _physical_device_properties; }

    glm::ivec2 get_window_extent() const { return {_window_extent.width, _window_extent.height}; }
    uint32_t get_current_frame() const { return _current_frame; }

    VkDescriptorPool get_descriptor_pool() const { return _descriptor_pool; }

    Array<VkDescriptorSetLayout, 4> get_descriptor_set_layouts() {
        return {
            _main_descriptor_set_layout,
            _texture_descriptor_set_layout,
            _buffer_descriptor_set_layout,
            _lighting_descriptor_set_layout
        };
    }

    Array<VkDescriptorSet, 4> get_descriptor_sets_for_frame(int frame_idx) {
        return {
            _main_descriptor_set.set_per_frame[frame_idx],
            _texture_descriptor_set.set_per_frame[frame_idx],
            _buffer_descriptor_set.set_per_frame[frame_idx],
            _lighting_descriptor_set.set_per_frame[frame_idx]
        };
    }

    Array<VkDescriptorSet, 4> get_descriptor_sets_for_current_frame() {
        return get_descriptor_sets_for_frame(_current_frame);
    }

    VkDescriptorSetLayout get_main_descriptor_set_layout() const { return _main_descriptor_set_layout; }
    const DescriptorSet& get_main_descriptor_set() const { return _main_descriptor_set; }
    VkDescriptorSetLayout get_texture_descriptor_set_layout() const { return _texture_descriptor_set_layout; }
    const DescriptorSet& get_texture_descriptor_set() const { return _texture_descriptor_set; }
    VkDescriptorSetLayout get_buffer_descriptor_set_layout() const { return _buffer_descriptor_set_layout; }
    const DescriptorSet& get_buffer_descriptor_set() const { return _buffer_descriptor_set; }
    VkDescriptorSetLayout get_lighting_descriptor_set_layout() const { return _lighting_descriptor_set_layout; }
    const DescriptorSet& get_lighting_descriptor_set() const { return _lighting_descriptor_set; }

    tracy::VkCtx* get_current_tracy_graphics_context() { return _graphics_queue_tracy_ctx[_current_frame]; }
    tracy::VkCtx* get_current_tracy_compute_context() { return _compute_queue_tracy_ctx[_current_frame]; }

    void init();
    void cleanup();
    void begin_frame();
    void render();
    void end_frame();

    void respond_to_window_event(SDL_Event* e);

    void wait_until_device_idle();

    Buffer create_dynamic_render_buffer(VkBufferUsageFlags buffer_usage, size_t initial_size);
    void create_or_resize_dynamic_buffer(VkBufferUsageFlags buffer_usage, uint32_t cur_frame, size_t new_size, DynamicBuffer& db);
    Buffer create_vertex_buffer(const void* data, size_t size);
    Buffer create_index_buffer(const void* data, size_t size);
    UniformBuffer create_uniform_buffer(size_t size);
    StorageBuffer create_storage_buffer(size_t size);

    void destroy_buffer(const Buffer& buffer);
    void destroy_dynamic_buffer(const DynamicBuffer& buffer);
    void destroy_uniform_buffer(const UniformBuffer& uniform_buffer);

    void* get_mapped_pointer(const Buffer& buffer);
    void* get_mapped_pointer(const DynamicBuffer& dynamic_buffer, uint32_t cur_frame);
    void* get_mapped_pointer(const UniformBuffer& uniform_buffer, uint32_t cur_frame);

    void upload_to_gpu(const ImageCpuData& image_cpu, VkFormat format, Image& image);
    void upload_to_gpu(VkCommandBuffer cmd_buffer, const ImageCpuData& image_cpu, VkFormat format, Image& image, Buffer& staging_buffer);

    Shader load_shader_from_file(const std::string& filename, ShaderType type);
    void destroy_shader(const Shader& shader);

    Ref<Image> load_image_from_file(const std::string& filename, VkFormat format);
    Ref<Texture> create_texture(const Image& image, VkFormat format);
    Ref<Texture> create_texture(const Image& image, VkFormat format, VkSamplerCreateInfo sampler_info);

    VkViewport default_viewport();
    VkRect2D default_scissor();

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);

    /// Create a static buffer to be uploaded on the GPU.
    /// Uses single-time commands and auto-deletes staging buffer, don't use this for performance.
    Buffer create_static_render_buffer_from_cpu(VkBufferUsageFlags buffer_usage, const void* data, size_t size);

    /// Create a static buffer to be uploaded on the GPU.
    /// Uses given command buffer and also gives you the staging buffer (which you need to delete later).
    void create_static_render_buffer_from_cpu(VkCommandBuffer command_buffer,
        VkBufferUsageFlags buffer_usage, const void* data, size_t size,
        Buffer& staging_buffer, Buffer& dest_buffer);

    void update_uniform_buffer(uint32_t cur_frame, const UniformBuffer& buffer, void* data, size_t size);
    void update_storage_buffer(uint32_t cur_frame, const DescriptorSet& descriptor_set, const StorageBuffer& buffer, void* data, size_t size);

    void render_imgui();

private:
    struct SDL_Window* _window;
    ECS* _ecs;
    Entity _camera_object;

    Vector<RenderInterface*> _render_interfaces;

    TerrainRenderer* _terrain_renderer;

    LightingBuffer _lighting;

    bool _is_initialized = false;
    int _frame_number = 0;
    VkExtent2D _window_extent = { 1920, 1080 };
    VkPhysicalDeviceProperties _physical_device_properties;

    VkInstance _instance{};
    VkDebugUtilsMessengerEXT _debug_messenger{};
    VkPhysicalDevice _physical_device{};
    VkDevice _device{};
    VkSurfaceKHR _surface{};
    VkQueue _graphics_queue{};
    VkQueue _compute_queue{};
    VkQueue _present_queue{};
    VkSwapchainKHR _swapchain{};
    Vector<VkImage> _swapchain_images;
    Vector<VkImageView> _swapchain_imageviews;

    VkDescriptorSetLayout _main_descriptor_set_layout{};
    DescriptorSet _main_descriptor_set{};

    VkDescriptorSetLayout _texture_descriptor_set_layout;
    DescriptorSet _texture_descriptor_set;

    VkDescriptorSetLayout _buffer_descriptor_set_layout;
    DescriptorSet _buffer_descriptor_set;

    VkDescriptorSetLayout _lighting_descriptor_set_layout;
    DescriptorSet _lighting_descriptor_set;

    VkCommandPool _command_pool;
    Vector<VkCommandBuffer> _command_buffers;

    VkDescriptorPool _descriptor_pool;

    Vector<VkSemaphore> _image_available_semaphores;
    Vector<VkSemaphore> _render_finished_semaphores;
    Vector<VkFence> _in_flight_fences;
    bool _framebuffer_resized = false;
    bool _framebuffer_minimized = false;
    uint32_t _current_frame = 0;

    int _queue_family_main_idx;
    SwapchainSupport _swapchain_support{};
    SwapchainSettings _swapchain_settings{};

    VmaAllocator _vma_allocator{};

    UniformBuffer _uniform_buffer;
    StorageBuffer _material_buffer, _lighting_buffer;

    Image _depth_image;
    VkImageView _depth_imageview{};

    Vector<tracy::VkCtx*> _graphics_queue_tracy_ctx;
    Vector<tracy::VkCtx*> _compute_queue_tracy_ctx;

    void create_instance();
    void create_device();
    void create_surface();
    void create_swapchain();
    void create_descriptor_set_layout();
    void create_graphics_pipeline();
    void create_command_pool();
    void create_depth_resources();
    void create_allocator();
    void create_uniform_buffers();
    void create_storage_buffers();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain();

    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);

    VkImageView create_imageview(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT);

    void recreate_swapchain();

    uint32_t find_memory_type(uint32_t type, VkMemoryPropertyFlags properties);

    void update_uniform_buffer(uint32_t cur_image);
    void update_texture_descriptor_sets(uint32_t cur_image);
    void update_material_buffer_descriptor_sets(uint32_t cur_image);
    void update_lighting_buffer_descriptor_sets(uint32_t cur_image);

    void copy_buffer(VkCommandBuffer cmd_buffer, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
    void transition_image_layout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_buffer_to_image(VkCommandBuffer cmd_buffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void _toposort_visit(RenderInterface* ri, Vector<RenderInterface*>& sorted);
    void toposort_render_interfaces();
};