#pragma once

#include <string>

#include "Az3D/InstanceStatic.hpp"

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

// Dynamic, per-frame object data
struct InstanceSkinned {
    InstanceSkinned() = default;

    InstanceStatic instanceStatic;

    std::vector<glm::mat4> boneMatrices;
};


struct InstanceSkinnedGroup {

};

} // namespace Az3D
