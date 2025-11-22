#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Tr3DWrap {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

namespace tinyRT {

struct Transform3D {
    const glm::mat4& getLMat4() const {
        if (localDirty) {
            local = glm::mat4(1.0f);
            local = glm::translate(local, localWrap.position);
            local *= glm::mat4_cast(localWrap.rotation);
            local = glm::scale(local, localWrap.scale);
            localDirty = false;
        }
        return local;
    }

    Tr3DWrap& getLWrap() {
        localDirty = true;
        worldDirty = true;
        return localWrap;
    }

    const glm::mat4& getWMat4(const Transform3D* parent) const {
        if (!worldDirty && !(parent && parent->worldDirty)) return world;

        glm::mat4 parentMat = parent ? parent->world : glm::mat4(1.0f);
        world = parentMat * getLMat4();
        worldDirty = false;
    }

    const glm::mat4& getWMat4() const {
        return world;
    }

private:
    Tr3DWrap localWrap;
    mutable glm::mat4 local{1.0f};
    mutable bool localDirty{true};

    mutable glm::mat4 world{1.0f};
    mutable bool worldDirty{true};
};

struct Transform2D {
    
};

}

using rtTransform3D = tinyRT::Transform3D;
using rtTRANF3D = tinyRT::Transform3D;

using rtTransform2D = tinyRT::Transform2D;
using rtTRANF2D = tinyRT::Transform2D;