#pragma once

#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

#include "TinyData/TinySkeleton.hpp"

struct TinySkeletonRT {
    TinyHandle skeleHandle; // Handle to TinySkeleton in fsRegistry
    const TinySkeleton* skeleton = nullptr;

    TinySkeletonRT() = default;
    ~TinySkeletonRT() = default;

    TinySkeletonRT(const TinySkeletonRT&) = delete;
    TinySkeletonRT& operator=(const TinySkeletonRT&) = delete;

    TinySkeletonRT(TinySkeletonRT&&) = default;
    TinySkeletonRT& operator=(TinySkeletonRT&&) = default;

    // Bone runtime data
    void init(TinyHandle skeletonHandle, const TinySkeleton& skeleton);

    std::vector<glm::mat4> localPose;
    std::vector<glm::mat4> finalPose;
    std::vector<glm::mat4> skinData;
    void refresh(uint32_t boneIndex, bool reupdate = true);
    void refreshAll();

    // Vulkan resources for skinning
    TinyVK::DescSet    descSet;
    TinyVK::DataBuffer skinBuffer;
    void vkCreate(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout);

    void updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform);

    // Global update
    void update();
};