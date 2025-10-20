#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

struct TinyBone {
    std::string name;

    int parent = -1; // -1 if root
    std::vector<int> children;

    glm::mat4 inverseBindMatrix = glm::mat4(1.0f); // From mesh space to bone local space
    glm::mat4 localBindTransform = glm::mat4(1.0f); // Local transform in bind pose
};

struct TinySkeleton {
    TinySkeleton() = default;

    TinySkeleton(const TinySkeleton&) = delete;
    TinySkeleton& operator=(const TinySkeleton&) = delete;

    TinySkeleton(TinySkeleton&&) = default;
    TinySkeleton& operator=(TinySkeleton&&) = default;

    std::string name;
    std::vector<TinyBone> bones;

    void clear() { 
        name.clear();
        bones.clear(); 
    }
    uint32_t insert(const TinyBone& bone) {
        bones.push_back(bone);
        return static_cast<uint32_t>(bones.size() - 1);
    }
};

struct TinySkeletonRT {
    TinySkeletonRT() = default;
    ~TinySkeletonRT() = default;

    TinySkeletonRT(const TinySkeletonRT&) = delete;
    TinySkeletonRT& operator=(const TinySkeletonRT&) = delete;

    TinySkeletonRT(TinySkeletonRT&&) = default;
    TinySkeletonRT& operator=(TinySkeletonRT&&) = default;

    // Create SoA copy of TinySkeleton for runtime use
    std::vector<std::string> boneNames;
    std::vector<int> boneParents;
    std::vector<std::vector<int>> boneChildren;
    std::vector<glm::mat4> boneInverseBindMatrices;
    std::vector<glm::mat4> boneLocalBindTransforms;

    std::vector<glm::mat4> boneTransformsFinal; // Final bone transforms for skinning
    TinyVK::DataBuffer boneFinalBuffer; // GPU buffer for final bone transforms

    TinyVK::DescSet boneDescSet; // Descriptor set for skinning shader usage

    // Initialization and creation
    void set(const TinySkeleton& skeleton);
    void vkCreate(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descSetLayout);

    // Runtime update
    void update();
};