#pragma once

#include <cstdint>

template <class T> constexpr uint32_t type_id() { return 0; }

class Image;
class Texture;
class Material;
class TexturedMesh;

template <> constexpr uint32_t type_id<Image>() { return 1; }
template <> constexpr uint32_t type_id<Texture>() { return 2; }
template <> constexpr uint32_t type_id<Material>() { return 3; }
template <> constexpr uint32_t type_id<TexturedMesh>() { return 4; }
