#pragma once

#include "tinySkeleton.hpp"
#include "tinyPool.hpp"

namespace tinyRT {

struct Skeleton3D {
    void init(const tinyPool<tinySkeleton>* pool, tinyHandle handle) {
        pool_ = pool;
        handle_ = handle;

        const tinySkeleton* skeleton = rSkeleton();
        if (!skeleton) return;

        localPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
        finalPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
        skinData_.resize(skeleton->bones.size(), glm::mat4(1.0f));

        // Initialize local pose to bind pose
        for (size_t i = 0; i < skeleton->bones.size(); ++i) {
            localPose_[i] = skeleton->bones[i].bindPose;
        }
    }

    void copy(const Skeleton3D* other) {
        if (!other) return;

        pool_ = other->pool_;
        handle_ = other->handle_;

        localPose_ = other->localPose_;
        finalPose_ = other->finalPose_;
        skinData_ = other->skinData_;
    }

    void update(uint32_t boneIdx = 0) noexcept {
        // If boneIdx is 0, traverse linearly
        const tinySkeleton* skeleton = rSkeleton();
        if (!skeleton || boneIdx >= skeleton->bones.size()) return;

        if (boneIdx == 0) {
            // Linear update
            for (size_t i = 0; i < skeleton->bones.size(); ++i) {
                const tinyBone& bone = skeleton->bones[i];

                glm::mat4 parentTransform = (bone.parent != -1) ? finalPose_[bone.parent] : glm::mat4(1.0f);

                finalPose_[i] = parentTransform * localPose_[i];
                skinData_[i] = finalPose_[i] * bone.bindInverse;
            }
        } else {
            std::function<void(uint32_t, const glm::mat4&)> recursiveUpdate =
            [&](uint32_t index, const glm::mat4& parentTransform) {
                if (index >= skeleton->bones.size()) return;

                const tinyBone& bone = skeleton->bones[index];

                finalPose_[index] = parentTransform * localPose_[index];
                skinData_[index] = finalPose_[index] * bone.bindInverse;

                for (int childIndex : bone.children) {
                    recursiveUpdate(childIndex, finalPose_[index]);
                }
            };

            // Retrieve parent
            glm::mat4 parentTransform = glm::mat4(1.0f);
            if (skeleton->bones[boneIdx].parent != -1) {
                parentTransform = finalPose_[skeleton->bones[boneIdx].parent];
            }

            recursiveUpdate(boneIdx, parentTransform);
        }
    }

    inline const tinySkeleton* rSkeleton() const noexcept {
        return pool_ && handle_ ? pool_->get(handle_) : nullptr;
    }

    inline tinyHandle skeleHandle() const noexcept {
        return handle_;
    }

    glm::mat4& localPose(uint32_t boneIndex) noexcept { return localPose_.at(boneIndex); }

    inline const std::vector<glm::mat4>& skinData() const noexcept { return skinData_; }

    void refresh(uint32_t boneIndex, bool recursive = false) {
        // Reset the local pose to the bind pose
        const tinySkeleton* skeleton = rSkeleton();
        if (!skeleton || boneIndex >= skeleton->bones.size()) return;

        localPose_[boneIndex] = skeleton->bones[boneIndex].bindPose;

        if (!recursive) return;

        std::function<void(uint32_t)> refreshRec = [&](uint32_t index) {
            if (index >= skeleton->bones.size()) return;

            localPose_[index] = skeleton->bones[index].bindPose;

            for (int childIndex : skeleton->bones[index].children) {
                refreshRec(childIndex);
            }
        };
        refreshRec(boneIndex);
    }

private:
    const tinyPool<tinySkeleton>* pool_ = nullptr;
    tinyHandle handle_;

    std::vector<glm::mat4> localPose_;
    std::vector<glm::mat4> finalPose_;
    std::vector<glm::mat4> skinData_;
};

};

using rtSKELE3D = tinyRT::Skeleton3D;
using rtSkeleton3D = tinyRT::Skeleton3D;