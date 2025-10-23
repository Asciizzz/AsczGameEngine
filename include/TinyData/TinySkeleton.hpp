#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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


// Runtime skeleton data for skinning
#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

struct TinySkeletonRT {
    TinyHandle skeleHandle; // Handle to TinySkeleton in fsRegistry
    const TinySkeleton* skeleton = nullptr;

    // Default constructor means this skeleton is f*cked
    TinySkeletonRT() : vkValid(false) {}
    TinySkeletonRT(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout);

    ~TinySkeletonRT() = default;

    TinySkeletonRT(const TinySkeletonRT&) = delete;
    TinySkeletonRT& operator=(const TinySkeletonRT&) = delete;

    TinySkeletonRT(TinySkeletonRT&&) = default;
    TinySkeletonRT& operator=(TinySkeletonRT&&) = default;

    // Bone runtime data
    void set(TinyHandle skeleHandle, const TinySkeleton* skeleton);
    void copy(const TinySkeletonRT& other);

    std::vector<glm::mat4> localPose;
    std::vector<glm::mat4> finalPose;
    std::vector<glm::mat4> skinData;
    void refresh(uint32_t boneIndex, bool reupdate = true);
    void refreshAll();

    // Global update
    void update();

    VkDescriptorSet descSet() const { return descSet_; }
    bool hasSkeleton() const { return vkValid && skeleton != nullptr; }

private:
    bool vkValid = false;

    const TinyVK::Device* deviceVK = nullptr;
    TinyVK::DescSet    descSet_;
    TinyVK::DataBuffer skinBuffer_;

    void vkCreate();
    
    void updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform);
};