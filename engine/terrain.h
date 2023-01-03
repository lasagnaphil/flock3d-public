#pragma once

#include "core/vector.h"

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "render/renderer.h"

class Renderer;

struct Terrain {
    float scale = 4.0f;

    int32_t octaves = 4;
    int32_t seed = 0;
    float persistance = 0.25f;
    float lacunarity = 2.0f;

    float chunk_width = 10.0f;
    float height_multiplier = 20.0f;

    Vector<int> chunk_sizes = {31, 63, 127};
};

struct TerrainPushConstants {
    glm::mat4 view;

    float in_scale;
    int32_t octaves;
    uint32_t seed;
    int _padding;

    float persistance;
    float lacunarity;

    float out_scale_width;
    float out_scale_height;

    void set_config(const Terrain& cfg) {
        in_scale = cfg.scale;

        octaves = cfg.octaves;
        seed = cfg.seed;
        persistance = cfg.persistance;
        lacunarity = cfg.lacunarity;

        out_scale_width = cfg.chunk_width;
        out_scale_height = cfg.height_multiplier;
    }
};

struct TerrainChunkTemplate {
    int grid_size;
    Buffer vbo, vbo_inst, ibo;

    Vector<glm::vec2> uvs;
    Vector<glm::u16vec3> triangles;
    Buffer vbo_staging_buffer, ibo_staging_buffer;
};

struct TerrainChunk {
    glm::ivec2 pos;
    int tmpl_idx;
};

class TerrainRenderer : public RenderInterface {
public:
    TerrainRenderer(Renderer* renderer, Terrain* terrain);

    void init();

    void create_graphics_pipeline();
    void begin_frame() override;
    void render(VkCommandBuffer command_buffer) override;
    void cleanup();

private:
    Terrain* _terrain;
    Entity _camera_object;

    Vector<TerrainChunk> _chunks;
    Vector<TerrainChunkTemplate> _chunk_templates;

    VkPipelineLayout _graphics_pipeline_layout;
    VkPipeline _graphics_pipeline;
};
