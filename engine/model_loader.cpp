#include "model_loader.h"

#include "render/renderer.h"
#include "glm/gtc/type_ptr.hpp"
#include "res.h"
#include "core/log.h"
#include "core/file.h"
#include "core/map.h"
#include "core/storage.h"
#include "stb/stb_image.h"
#include "vulkan/vulkan_core.h"

#include <cgltf.h>
#include <physfs.h>

#define TRACY_ENABLE
#include "tracy/Tracy.hpp"

enum class ImageType {
    BaseColor, MetallicRoughness, AO
};

struct GLTFImageCpuData : public ImageCpuData {
    ImageType type;
    std::string path;
    Ref<Image> image_id;
    Ref<Texture> texture_id;
};

struct MaterialCpuData {
    Ref<GLTFImageCpuData> base_color_texture_id;
    Ref<GLTFImageCpuData> metallic_roughness_texture_id;
    Ref<GLTFImageCpuData> ao_texture_id;

    glm::vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float ao_factor;
};

struct TexturedMeshCpuData {
    Vector<TexturedVertex> vertices;
    Vector<uint16_t> indices;

    MaterialCpuData material;

    glm::vec3 aabb_min;
    glm::vec3 aabb_max;

    std::string model_name;

    void load_geometry_from_obj(const std::string& obj_filename);

    void calc_bounds();
};

ModelLoader::ModelLoader(Renderer *renderer) : _renderer(renderer) {}

static PHYSFS_EnumerateCallbackResult select_gltf_model_files(void *data, const char *origdir, const char *fname)
{
    if (std::string(fname).rfind(".blend") != std::string::npos) return PHYSFS_ENUM_OK;
    
    auto fnames = (Vector<std::string>*)data;
    fnames->push_back(fname);
    return PHYSFS_ENUM_OK;  // give me more data, please.
}

void ModelLoader::load(const char* path) {
    ZoneScoped;

    auto res = Res::inst();

    Vector<std::string> model_names;

    PHYSFS_enumerate(path, select_gltf_model_files, (void*)&model_names);

    Storage<GLTFImageCpuData> image_data_pool;

    // Add default white 1x1 texture
    uint32_t ffdata = 0xffffffff;
    auto [default_base_color_img_data_id, default_base_color_img_data] = image_data_pool.emplace();
    default_base_color_img_data->pixels = (unsigned char*)&ffdata;
    default_base_color_img_data->width = 1;
    default_base_color_img_data->height = 1;
    default_base_color_img_data->channels = 4;
    default_base_color_img_data->data_channels = 4;
    default_base_color_img_data->type = ImageType::BaseColor;

    auto [default_mr_img_data_id, default_mr_img_data] = image_data_pool.emplace();
    default_mr_img_data->pixels = (unsigned char*)&ffdata;
    default_mr_img_data->width = 1;
    default_mr_img_data->height = 1;
    default_mr_img_data->channels = 2;
    default_mr_img_data->data_channels = 2;
    default_mr_img_data->type = ImageType::MetallicRoughness;

    auto [default_ao_img_data_id, default_ao_img_data] = image_data_pool.emplace();
    default_ao_img_data->pixels = (unsigned char*)&ffdata;
    default_ao_img_data->width = 1;
    default_ao_img_data->height = 1;
    default_ao_img_data->channels = 1;
    default_ao_img_data->data_channels = 1;
    default_ao_img_data->type = ImageType::AO;

    ParallelMap<std::string, Ref<GLTFImageCpuData>> cached_images;

    Vector<TexturedMeshCpuData> loaded_meshes;

    for (int gltf_idx = 0; gltf_idx < model_names.size(); gltf_idx++) {
        std::string model_name = model_names[gltf_idx];
        Model model;
        model.name = model_name;
        _models.insert({model_name, model});

        auto gltf_basepath = std::string(path) + "/" + model_name;
        auto gltf_filename = gltf_basepath + "/scene.gltf";
        auto gltf_str = load_file_to_string(gltf_filename);
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        cgltf_result cgltf_parse_result = cgltf_parse(&options, gltf_str.data(), gltf_str.size(), &data);
        if (cgltf_parse_result != cgltf_result_success) {
            log_error("Failed to load gltf file {}!", gltf_filename);
            continue;
        }
        Vector<uint8_t> buf;
        if (data->buffers_count == 1) {
            auto buf_path = std::string(gltf_basepath) + "/" + data->buffers[0].uri;
            buf = load_file_to_buffer(buf_path);
        }
        else if (data->buffers_count > 1) {
            log_error("gltf with multiple buffers not supported!");
            continue;
        }
        for (int mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
            auto& mesh = data->meshes[mesh_idx];
            for (int prim_idx = 0; prim_idx < mesh.primitives_count; prim_idx++) {
                auto& out_mesh = loaded_meshes.push_empty();
                out_mesh.model_name = model_name;

                auto& prim = mesh.primitives[prim_idx];
                const int prim_count = prim.attributes[0].data->count;
                for (int attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++) {
                    if (prim_count != prim.attributes[attr_idx].data->count) {
                        log_error("gltf with different counts per attribute not supported!");
                        loaded_meshes.pop_back();
                        goto gltf_finalize;
                    }
                }

                auto& mat = *prim.material;
                if (mat.has_pbr_metallic_roughness) {
                    auto& pbr_mr = mat.pbr_metallic_roughness;
                    if (pbr_mr.base_color_texture.texture) {
                        auto img_path = std::string(gltf_basepath) + "/" + pbr_mr.base_color_texture.texture->image->uri;
                        auto it = cached_images.find(img_path);
                        if (it != cached_images.end()) {
                            out_mesh.material.base_color_texture_id = it->second;
                        }
                        else {
                            auto [img_id, img] = image_data_pool.emplace();
                            img->data_channels = 4;
                            img->type = ImageType::BaseColor;
                            img->path = img_path;
                            out_mesh.material.base_color_texture_id = img_id;
                            cached_images.insert({img_path, img_id});
                        }
                    }
                    else {
                        out_mesh.material.base_color_texture_id = default_base_color_img_data_id;
                    }
                    if (pbr_mr.metallic_roughness_texture.texture) {
                        auto img_path = std::string(gltf_basepath) + "/" + pbr_mr.metallic_roughness_texture.texture->image->uri;
                        auto it = cached_images.find(img_path);
                        if (it != cached_images.end()) {
                            out_mesh.material.metallic_roughness_texture_id = it->second;
                        }
                        else {
                            auto [img_id, img] = image_data_pool.emplace();
                            img->data_channels = 2;
                            img->type = ImageType::MetallicRoughness;
                            img->path = img_path;
                            out_mesh.material.metallic_roughness_texture_id = img_id;
                            cached_images.insert({img_path, img_id});
                        }
                    }
                    else {
                        out_mesh.material.metallic_roughness_texture_id = default_mr_img_data_id;
                    }
                    out_mesh.material.base_color_factor = glm::make_vec4(mat.pbr_metallic_roughness.base_color_factor);
                    out_mesh.material.metallic_factor = mat.pbr_metallic_roughness.metallic_factor;
                    out_mesh.material.roughness_factor = mat.pbr_metallic_roughness.roughness_factor;
                }
                if (mat.occlusion_texture.texture) {
                    auto img_path = std::string(gltf_basepath) + "/" + mat.occlusion_texture.texture->image->uri;
                    auto it = cached_images.find(img_path);
                    if (it != cached_images.end()) {
                        out_mesh.material.ao_texture_id = it->second;
                    }
                    else {
                        auto [img_id, img] = image_data_pool.emplace();
                        img->data_channels = 1;
                        img->type = ImageType::AO;
                        img->path = img_path;
                        out_mesh.material.ao_texture_id = img_id;
                        cached_images.insert({img_path, img_id});
                    }
                }
                else {
                    out_mesh.material.ao_texture_id = default_ao_img_data_id;
                }
                out_mesh.material.ao_factor = 1.0f;

                out_mesh.vertices.resize(prim_count);
                for (int attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++) {
                    auto& attr = prim.attributes[attr_idx];
                    auto& accessor = *attr.data;
                    auto& buffer_view = *accessor.buffer_view;
                    const uint8_t* p;
                    if (buffer_view.buffer->data) {
                        p = reinterpret_cast<uint8_t*>(buffer_view.buffer->data) + buffer_view.offset + accessor.offset;
                    }
                    else {
                        p = buf.data() + buffer_view.offset + accessor.offset;
                    }
                    if (attr.type == cgltf_attribute_type_position) {
                        for (int i = 0; i < prim_count; i++) {
                            memcpy(&out_mesh.vertices[i].pos, p, 12);
                            p += accessor.stride;
                        }
                    }
                    else if (attr.type == cgltf_attribute_type_normal) {
                        for (int i = 0; i < prim_count; i++) {
                            memcpy(&out_mesh.vertices[i].normal, p, 12);
                            p += accessor.stride;
                        }
                    }
                    else if (attr.type == cgltf_attribute_type_texcoord) {
                        for (int i = 0; i < prim_count; i++) {
                            memcpy(&out_mesh.vertices[i].texcoord, p, 8);
                            p += accessor.stride;
                        }
                    }
                }

                out_mesh.indices.resize(prim.indices->count);

                auto& buffer_view = *prim.indices->buffer_view;
                if (prim.indices->component_type == cgltf_component_type_r_16u) {
                    const uint8_t* p;
                    if (buffer_view.buffer->data) {
                        p = reinterpret_cast<uint8_t*>(buffer_view.buffer->data) + buffer_view.offset + prim.indices->offset;
                    }
                    else {
                        p = buf.data() + buffer_view.offset + prim.indices->offset;
                    }
                    memcpy(out_mesh.indices.data(), p, 2 * prim.indices->count);
                }
                else if (prim.indices->component_type == cgltf_component_type_r_32u) {
                    log_error("Cannot support GLTF mesh with 32 bit indices! Check if the vertex count is <65535 when exporting.");
                    loaded_meshes.pop_back();
                    goto gltf_finalize;
                }
            }
        }
        gltf_finalize:
        cgltf_free(data);
    }

    // Load all textures and upload to GPU
    VkCommandBuffer cmd_buffer = _renderer->begin_single_time_commands();
    Vector<Buffer> staging_buffers;
    staging_buffers.reserve(image_data_pool.size());
    image_data_pool.foreach_with_ref([&](Ref<GLTFImageCpuData> img_data_id, GLTFImageCpuData& img_data) {
        if (!img_data.path.empty()) {
            img_data.load_from_file(img_data.path, img_data.data_channels);
        }
        Image* image;
        std::tie(img_data.image_id, image) = res->_image_pool.emplace();
        auto& staging_buffer = staging_buffers.push_empty();
        VkFormat format;
        if (img_data.type == ImageType::BaseColor) {
            format = VK_FORMAT_R8G8B8A8_SRGB;
        }
        else if (img_data.type == ImageType::MetallicRoughness) {
            format = VK_FORMAT_R8G8_SNORM;
        }
        else if (img_data.type == ImageType::AO) {
            format = VK_FORMAT_R8_SNORM;
        }
        _renderer->upload_to_gpu(cmd_buffer, img_data, format, *image, staging_buffer);
        img_data.texture_id = _renderer->create_texture(*image, format);
    });

    // Load all mesh data and upload to GPU
    for (auto& mesh_cpu : loaded_meshes) {
        mesh_cpu.aabb_min = glm::vec3(FLT_MAX);
        mesh_cpu.aabb_max = glm::vec3(FLT_MIN);
        for (auto &vert : mesh_cpu.vertices) {
            mesh_cpu.aabb_min = glm::min(mesh_cpu.aabb_min, vert.pos);
            mesh_cpu.aabb_max = glm::max(mesh_cpu.aabb_max, vert.pos);
        }
        auto [mesh_id, mesh] = res->_textured_mesh_pool.emplace();
        Buffer vertex_staging_buffer, index_staging_buffer;
        _renderer->create_static_render_buffer_from_cpu(cmd_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            mesh_cpu.vertices.data(), mesh_cpu.vertices.size() * sizeof(TexturedVertex),
            vertex_staging_buffer, mesh->vertex_buffer);
        _renderer->create_static_render_buffer_from_cpu(cmd_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            mesh_cpu.indices.data(), mesh_cpu.indices.size() * sizeof(uint16_t),
            index_staging_buffer, mesh->index_buffer);
        mesh->vertex_count = mesh_cpu.vertices.size();
        mesh->index_count = mesh_cpu.indices.size();
        mesh->aabb_min = mesh_cpu.aabb_min;
        mesh->aabb_max = mesh_cpu.aabb_max;
        auto [mat_id, mat] = res->_material_pool.emplace();
        mesh->mat_id = mat_id;
        GLTFImageCpuData* albedo_img = image_data_pool.get(mesh_cpu.material.base_color_texture_id);
        GLTFImageCpuData* mr_img = image_data_pool.get(mesh_cpu.material.metallic_roughness_texture_id);
        GLTFImageCpuData* ao_img = image_data_pool.get(mesh_cpu.material.ao_texture_id);
        mat->albedo = mesh_cpu.material.base_color_factor;
        mat->metallic = mesh_cpu.material.metallic_factor;
        mat->roughness = mesh_cpu.material.roughness_factor;
        mat->albedo_tex_id = albedo_img->texture_id;
        mat->metallic_roughness_tex_id = mr_img->texture_id;
        mat->ao_tex_id = ao_img->texture_id;
        staging_buffers.push_back(vertex_staging_buffer);
        staging_buffers.push_back(index_staging_buffer);

        Model& model = _models.at(mesh_cpu.model_name);
        model.meshes.push_back(mesh_id);

        if (model.textures.find(albedo_img->texture_id) == nullptr) {
            model.textures.push_back(albedo_img->texture_id);
        }
        if (model.textures.find(mr_img->texture_id) == nullptr) {
            model.textures.push_back(mr_img->texture_id);
        }
        if (model.textures.find(ao_img->texture_id) == nullptr) {
            model.textures.push_back(ao_img->texture_id);
        }
    }

    _renderer->end_single_time_commands(cmd_buffer);
    for (auto& staging_buffer : staging_buffers) {
        _renderer->destroy_buffer(staging_buffer);
    }
    staging_buffers.clear();

    image_data_pool.get(default_base_color_img_data_id)->pixels = nullptr;
    image_data_pool.get(default_mr_img_data_id)->pixels = nullptr;
    image_data_pool.get(default_ao_img_data_id)->pixels = nullptr;

    image_data_pool.foreach_with_ref([&](Ref<GLTFImageCpuData> img_data_id, GLTFImageCpuData& img_data) {
        img_data.cleanup();
    });
}

void ModelLoader::unload() {

}
