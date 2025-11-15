#include "tinyRT_Skeleton3D.hpp"

using namespace tinyVk;
using namespace tinyRT;

Skeleton3D* Skeleton3D::init(const tinyVk::Device* deviceVk, const tinyPool<tinySkeleton>* skelePool, VkDescriptorPool descPool, VkDescriptorSetLayout descSLayout, uint32_t maxFramesInFlight) {
    vkValid = true;

    deviceVk_ = deviceVk;
    skelePool_ = skelePool;
    maxFramesInFlight_ = maxFramesInFlight;

    descSet_.allocate(deviceVk->device, descPool, descSLayout);
    return this;
}

void Skeleton3D::set(tinyHandle skeletonHandle) {
    skeleHandle_ = skeletonHandle;

    const tinySkeleton* skeleton = rSkeleton();
    if (!vkValid || skeleton == nullptr) return;

    localPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
    finalPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
    skinData_.resize(skeleton->bones.size(), glm::mat4(1.0f));

    // Initialize local pose to bind pose
    for (size_t i = 0; i < skeleton->bones.size(); ++i) {
        localPose_[i] = skeleton->bones[i].bindPose;
    }

    vkWrite(deviceVk_, &skinBuffer_, &descSet_, maxFramesInFlight_, skinData_.size());
}

void Skeleton3D::copy(const Skeleton3D* other) {
    if (!other || !other->pValid()) return;

    skeleHandle_ = other->skeleHandle_; // Guaranteed to be valid since other is valid

    localPose_ = other->localPose_;
    finalPose_ = other->finalPose_;
    skinData_ = other->skinData_;

    vkWrite(deviceVk_, &skinBuffer_, &descSet_, maxFramesInFlight_, skinData_.size());
}


void Skeleton3D::vkWrite(const tinyVk::Device* deviceVk, tinyVk::DataBuffer* buffer, tinyVk::DescSet* descSet, size_t maxFramesInFlight, uint32_t boneCount) {
    if (boneCount == 0) return;

    VkDeviceSize perFrameSize = sizeof(glm::mat4) * boneCount;
    buffer
        ->setDataSize(perFrameSize * maxFramesInFlight)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVk)
        .mapMemory();

    DescWrite()
        .setDstSet(*descSet)
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            *buffer,
            0,
            perFrameSize
        } })
        .updateDescSets(deviceVk->device);
}

void Skeleton3D::refresh(uint32_t boneIndex, bool reupdate) {
    // Reinitialize local pose of specific bone to bind pose
    if (boneIndex >= rSkeleton()->bones.size()) return;

    localPose_[boneIndex] = rSkeleton()->bones[boneIndex].bindPose;
}

void Skeleton3D::refreshAll() {
    for (size_t i = 0; i < rSkeleton()->bones.size(); ++i) {
        localPose_[i] = rSkeleton()->bones[i].bindPose;
    }
    updateFlat();
}

void Skeleton3D::updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform) {
    if (boneIndex >= rSkeleton()->bones.size()) return;

    const tinyBone& bone = rSkeleton()->bones[boneIndex];

    finalPose_[boneIndex] = parentTransform * localPose_[boneIndex];
    skinData_[boneIndex] = finalPose_[boneIndex] * bone.bindInverse;

    for (int childIndex : bone.children) {
        updateRecursive(childIndex, finalPose_[boneIndex]);
    }
}

void Skeleton3D::updateFlat() {
    for (size_t i = 0; i < rSkeleton()->bones.size(); ++i) {
        const tinyBone& bone = rSkeleton()->bones[i];

        glm::mat4 parentTransform = glm::mat4(1.0f);
        if (bone.parent != -1) {
            parentTransform = finalPose_[bone.parent];
        }

        finalPose_[i] = parentTransform * localPose_[i];
        skinData_[i] = finalPose_[i] * bone.bindInverse;
    }
}

void Skeleton3D::update(uint32_t boneIndx) noexcept {
    if (!boneValid(boneIndx)) return;

    if (boneIndx == 0) {
        updateFlat();
    } else {
        // Retrieve parent
        glm::mat4 parentTransform = finalPose_[rSkeleton()->bones[boneIndx].parent];
        updateRecursive(boneIndx, parentTransform);
    }
}

void Skeleton3D::vkUpdate(uint32_t curFrame) noexcept {
    if (!pValid() || curFrame >= maxFramesInFlight_) return;

    // Upload updated skin data to GPU for the current frame
    VkDeviceSize offset = dynamicOffset(curFrame);
    VkDeviceSize dataSize = sizeof(glm::mat4) * skinData_.size();
    skinBuffer_.copyData(skinData_.data(), dataSize, offset);
}