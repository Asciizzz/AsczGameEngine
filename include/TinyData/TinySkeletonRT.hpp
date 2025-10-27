#pragma once

#include "TinyData/TinySkeleton.hpp"
#include "TinyExt/TinyRegistry.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

struct TinySkeletonRT {

    // Default constructor means this skeleton is f*cked
    TinySkeletonRT() = default;
    void init(const TinyVK::Device* deviceVK, const TinyRegistry* fsRegistry, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout);

    ~TinySkeletonRT() = default;

    TinySkeletonRT(const TinySkeletonRT&) = delete;
    TinySkeletonRT& operator=(const TinySkeletonRT&) = delete;

    TinySkeletonRT(TinySkeletonRT&&) = default;
    TinySkeletonRT& operator=(TinySkeletonRT&&) = default;

    // Bone runtime data
    void set(TinyHandle skeleHandle);
    void copy(const TinySkeletonRT* other);

    void refresh(uint32_t boneIndex, bool reupdate = true);
    void refreshAll();

    // Global update
    void update(uint32_t index = 0);

    VkDescriptorSet descSet() const { return pValid() ? descSet_.get() : VK_NULL_HANDLE; }
    uint32_t boneCount() const {
        return pValid() ? static_cast<uint32_t>(localPose_.size()) : 0;
    }
    bool boneValid(uint32_t index) const {
        return pValid() && index < boneCount();
    }

    glm::mat4 localPose(uint32_t index) const { return localPose_[index]; }
    glm::mat4& localPose(uint32_t index) { return localPose_[index]; }
    const glm::mat4& finalPose(uint32_t index) const { return finalPose_[index]; }
    const glm::mat4& bindPose(uint32_t index) const {
        const TinySkeleton* skeleton = rSkeleton();
        if (skeleton && index < skeleton->bones.size()) {
            return skeleton->bones[index].inverseBindMatrix;
        }
        static glm::mat4 identity = glm::mat4(1.0f);
        return identity;
    }

    void setLocalPose(uint32_t index, const glm::mat4& pose = glm::mat4(1.0f)) {
        if (index >= localPose_.size()) return;

        localPose_[index] = pose;
        update(index);
    }

    TinyHandle skeleHandle() const { return skeleHandle_; }

    const TinySkeleton* rSkeleton() const {
        return fsRegistry_ ? fsRegistry_->get<TinySkeleton>(skeleHandle_) : nullptr;
    }

    bool hasSkeleton() const {
        return rSkeleton() != nullptr;
    }

    bool pValid() const {
        return vkValid && hasSkeleton();
    }

private:
    bool vkValid = false;

    TinyHandle skeleHandle_;
    const TinyRegistry* fsRegistry_ = nullptr; // The entire filesystem registry (guarantees to avoid dangling pointers)

    std::vector<glm::mat4> localPose_;
    std::vector<glm::mat4> finalPose_;
    std::vector<glm::mat4> skinData_;

    const TinyVK::Device* deviceVK_ = nullptr;
    TinyVK::DescSet    descSet_;
    TinyVK::DataBuffer skinBuffer_;

    void vkCreate();
    
    void updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform);
    void updateFlat();
};