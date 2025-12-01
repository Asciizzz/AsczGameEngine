#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace tinyRT {

struct Transform3D {
    glm::mat4 local{1.0f};
    glm::mat4 world{1.0f};

    // In the future will have more advanced dirty flag system
};

}

using rtTransform3D = tinyRT::Transform3D;
using rtTRANFM3D = tinyRT::Transform3D;