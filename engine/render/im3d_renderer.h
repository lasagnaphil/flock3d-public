#pragma once

#include "renderer.h"

struct alignas(16) Im3dPushConstants {
    glm::mat4 projview;
    glm::vec4 color;
    glm::vec2 viewport_size;
    float prim_width;
    float blend_factor;
};

class Im3dRenderer : public RenderInterface {
public:
    enum class PrimType {
        Point, Line, Triangle, _PrimCount
    };
    static constexpr int PrimTypeCount = (int) PrimType::_PrimCount;

    Im3dRenderer(Renderer* renderer) : RenderInterface(renderer) {}

    void init();
    void new_frame();
    void render(VkCommandBuffer command_buffer) override;
    void cleanup();

    void set_point_color(glm::vec4 color);
    void set_point_size(float size);
    void set_point_blend_factor(float blend_factor);

    void set_line_color(glm::vec4 color);
    void set_line_width(float width);
    void set_line_blend_factor(float blend_factor);

    void set_tri_color(glm::vec4 color);

    void push_point(const glm::vec3& p);
    void push_line(const glm::vec3& p1, const glm::vec3& p2);
    void push_tri(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);

private:
    void create_graphics_pipelines();
    void create_buffers();

    void start_new_batch(PrimType primtype);
    void start_new_point_batch() { start_new_batch(PrimType::Point); }
    void start_new_line_batch() { start_new_batch(PrimType::Line); }
    void start_new_tri_batch() { start_new_batch(PrimType::Triangle); }

    bool _is_initialized = false;

    struct PipelineData {
        VkPipelineLayout graphics_pipeline_layout;
        VkPipeline graphics_pipeline;

        Vector<glm::vec3> positions;
        Vector<uint32_t> batch_starts;
        Vector<Im3dPushConstants> batch_data;
        Im3dPushConstants cur_batch_data;

        Buffer vbo;

        uint32_t max_prim_count;
        uint32_t max_batch_count;
    };

    PipelineData _pipeline_data[PrimTypeCount];

    PipelineData& get_pipeline_data(PrimType type) { return _pipeline_data[(int)type]; }
};