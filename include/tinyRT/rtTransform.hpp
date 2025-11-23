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
    Tr3D& local() {
        lDirty_ = true;
        local_.update(); // Late refresh (acceptable)
        return local_;
    }

    const glm::mat4& world() const {
        return world_;
    }

    const glm::mat4& recalcWorld(const Transform3D* parent) {
        if (wDirty_ || (parent && parent->wDirty_)) {
            world_ = parent->world() * local_.Mat4;
            wDirty_ = false;
            lDirty_ = false;
        }

        return world_;
    }

private:
    Tr3D local_;
    bool lDirty_ = true;

    glm::mat4 world_{1.0f};
    bool wDirty_ = true;
};

}

using rtTransform3D = tinyRT::Transform3D;
using rtTRANFM3D = tinyRT::Transform3D;