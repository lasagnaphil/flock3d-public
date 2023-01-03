#pragma once

#include <string>
#include "core/vector.h"
#include "core/storage.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraMovementFlags {
    CAM_FORWARD = 0x1,
    CAM_BACKWARD = 0x2,
    CAM_LEFT = 0x4,
    CAM_RIGHT = 0x8,
    CAM_UP = 0x10,
    CAM_DOWN = 0x20
};

class TexturedMesh;
class Texture;

struct Transform {
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;

    glm::mat4 to_matrix() const {
        glm::mat3 rot_mat = glm::mat3_cast(rotation);
        glm::mat4 m;
        m[0] = glm::vec4(scale * rot_mat[0], 0);
        m[1] = glm::vec4(scale * rot_mat[1], 0);
        m[2] = glm::vec4(scale * rot_mat[2], 0);
        m[3] = glm::vec4(translation, 1);
        return m;
    }

    void reset() {
        translation = glm::vec3(0);
        rotation = glm::identity<glm::quat>();
        scale = glm::vec3(1);
    }
};

struct TransformVel {
    glm::vec3 vel;
    glm::vec3 angvel;
};

struct Model {
    std::string name;
    Vector<Ref<TexturedMesh>> meshes;
    Vector<Ref<Texture>> textures;
};

struct Camera {
    glm::mat3 rotation;
	glm::vec3 position;
	glm::mat4 proj_mat;

    glm::mat4 get_view_matrix() const {
        // TODO: rewrite this without using GLM lookAt (Vulkan conventions are weird though...)
        return glm::lookAt(position, position + rotation[2], rotation[1]);
        // glm::mat3 R = glm::transpose(rotation);
        // glm::vec3 p = -(R * position);
        // glm::mat4 view_mat;
        // view_mat[0] = glm::vec4(R[0], 0);
        // view_mat[1] = glm::vec4(R[1], 0);
        // view_mat[2] = glm::vec4(R[2], 0);
        // view_mat[3] = glm::vec4(p, 1);
        // return view_mat;
    }
};

struct WireframeDebugRenderComp {
    glm::vec4 color;
    float width;
    float blend_factor;
};

