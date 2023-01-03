#include "res.h"

Res* Res::_inst = nullptr;

void Res::_cleanup() {
    _image_pool.release();
    _textured_mesh_pool.release();
}
