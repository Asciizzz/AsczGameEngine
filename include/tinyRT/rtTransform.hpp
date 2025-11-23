#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Tr3D {
    glm::vec3 T{0.0f, 0.0f, 0.0f};
    glm::quat R{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 S{1.0f, 1.0f, 1.0f};

    glm::mat4 Mat4{1.0f};
    void update() {
        Mat4 = glm::translate(glm::mat4(1.0f), T) *
               glm::mat4_cast(R) *
               glm::scale(glm::mat4(1.0f), S);
    }
};

namespace tinyRT {

struct Transform3D {
    Tr3D local;
    glm::mat4 world{1.0f};

    // In the future will have more advanced dirty flag system
};

}

using rtTransform3D = tinyRT::Transform3D;
using rtTRANFM3D = tinyRT::Transform3D;