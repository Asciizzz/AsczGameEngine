#pragma once

#include "tinyData/tinySkeleton.hpp"
#include "tinyRegistry.hpp"
#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

namespace tinyRT {

struct Skeleton3D {
    Skeleton3D() noexcept = default;
    Skeleton3D* init(const tinyVk::Device* deviceVk, const tinyPool<tinySkeleton>* skelePool, VkDescriptorPool descPool, VkDescriptorSetLayout descSLayout, uint32_t maxFramesInFlight);

    Skeleton3D(const Skeleton3D&) = delete;
    Skeleton3D& operator=(const Skeleton3D&) = delete;

    Skeleton3D(Skeleton3D&&) noexcept = default;
    Skeleton3D& operator=(Skeleton3D&&) noexcept = default;

// -----------------------------------------

    // Bone runtime data
    void set(tinyHandle skeleHandle);
    void copy(const Skeleton3D* other);

    void refresh(uint32_t boneIndex, bool reupdate = true);
    void refreshAll();

// -----------------------------------------

    static void vkWrite(const tinyVk::Device* device, tinyVk::DataBuffer* buffer, tinyVk::DescSet* descSet, size_t maxFramesInFlight, uint32_t boneCount);
    VkDescriptorSet descSet() const noexcept { return pValid() ? descSet_.get() : VK_NULL_HANDLE; }
    uint32_t dynamicOffset(uint32_t curFrame) const noexcept {
        return sizeof(glm::mat4) * boneCount() * curFrame;
    }

    uint32_t boneCount() const {
        return pValid() ? static_cast<uint32_t>(localPose_.size()) : 0;
    }
    bool boneValid(uint32_t index) const {
        return pValid() && index < boneCount();
    }

    glm::mat4 localPose(uint32_t index) const { return localPose_[index]; }
    glm::mat4& localPose(uint32_t index) { return localPose_[index]; }
    const glm::mat4& finalPose(uint32_t index) const { return finalPose_[index]; }

    glm::mat4 bindPose(uint32_t index) const {
        const tinySkeleton* skeleton = rSkeleton();
        if (!boneValid(index) || !skeleton) return glm::mat4(1.0f);
        else return skeleton->bones[index].bindPose;
    }

    void setLocalPose(uint32_t index, const glm::mat4& pose = glm::mat4(1.0f)) {
        if (index >= localPose_.size()) return;
        localPose_[index] = pose;
    }

    tinyHandle skeleHandle() const { return skeleHandle_; }

    const tinySkeleton* rSkeleton() const {
        return skelePool_ ? skelePool_->get(skeleHandle_) : nullptr;
    }

    bool hasSkeleton() const {
        return rSkeleton() != nullptr;
    }

    bool pValid() const {
        return vkValid && hasSkeleton();
    }

// -----------------------------------------

    void update(uint32_t boneIndx) noexcept;
    void vkUpdate(uint32_t curFrame) noexcept;

private:
    bool vkValid = false;

    tinyHandle skeleHandle_;
    const tinyPool<tinySkeleton>* skelePool_ = nullptr;
    const tinyVk::Device* deviceVk_ = nullptr;
    uint32_t maxFramesInFlight_ = 0;

    tinyVk::DescSet    descSet_;
    tinyVk::DataBuffer skinBuffer_;

    std::vector<glm::mat4> localPose_;
    std::vector<glm::mat4> finalPose_;
    std::vector<glm::mat4> skinData_;

    void updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform);
    void updateFlat();
};

} // namespace tinyRT

using tinyRT_SKEL3D = tinyRT::Skeleton3D;