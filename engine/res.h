#pragma once

#include "core/log.h"
#include "core/storage.h"

#include "render/renderer.h"

class Res {
public:
    static Res* _inst;

    static Res* initialize() {
        _inst = new Res();
        return _inst;
    }

    static Res* inst() {
        if (_inst == nullptr) {
            log_error("Trying to get an instance of Res before init!");
            std::abort();
        }
        return _inst;
    }

    static void cleanup() {
        if (_inst) _inst->_cleanup();
        _inst = nullptr;
    }

    template <class T>
    T* get(Ref<T> id);

    template <>
    Image* get(Ref<Image> image_id) { return _image_pool.get(image_id); }

    template <>
    TexturedMesh* get(Ref<TexturedMesh> mesh_id) { return _textured_mesh_pool.get(mesh_id); }

    template <>
    Texture* get(Ref<Texture> tex_id) { return _texture_pool.get(tex_id); }

    template <>
    Material* get(Ref<Material> mat_id) { return _material_pool.get(mat_id); }

    Storage<Image> _image_pool;
    Storage<TexturedMesh> _textured_mesh_pool;
    Storage<Texture> _texture_pool;
    Storage<Material> _material_pool;

    void _cleanup();
};
