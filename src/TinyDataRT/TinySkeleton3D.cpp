#include "tinyDataRT/tinySkeleton3D.hpp"

using namespace tinyVK;
using namespace tinyRT;

Skeleton3D* Skeleton3D::init(const tinyVK::Device* deviceVK, const tinyRegistry* fsRegistry, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout, uint32_t maxFramesInFlight) {
    vkValid = true;

    deviceVK_ = deviceVK;
    fsRegistry_ = fsRegistry;
    maxFramesInFlight_ = maxFramesInFlight;

    descSet_.allocate(deviceVK->device, descPool, descLayout);
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

    vkCreate();
}

void Skeleton3D::copy(const Skeleton3D* other) {
    if (!other || !other->pValid()) return;

    skeleHandle_ = other->skeleHandle_; // Guaranteed to be valid since other is valid

    localPose_ = other->localPose_;
    finalPose_ = other->finalPose_;
    skinData_ = other->skinData_;

    vkCreate();
}

void Skeleton3D::vkCreate() {
    if (!hasSkeleton()) return;

    // Create skinning data buffer
    VkDeviceSize preFrameSize = sizeof(glm::mat4) * skinData_.size();
    skinBuffer_
        .setDataSize(preFrameSize * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK_)
        .mapAndCopy(skinData_.data());

    // Update descriptor set with skin buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = skinBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = preFrameSize;

    DescWrite()
        .setDstSet(descSet_)
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSets(deviceVK_->device);
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

void Skeleton3D::update(uint32_t boneIdx, uint32_t curFrame) {
    if (!boneValid(boneIdx)) return;

    if (boneIdx == 0) {
        updateFlat();
    } else {
        // Retrieve parent
        glm::mat4 parentTransform = finalPose_[rSkeleton()->bones[boneIdx].parent];
        updateRecursive(boneIdx, parentTransform);
    }

    if (curFrame >= maxFramesInFlight_) return;

    // Upload updated skin data to GPU for the current frame
    size_t offset = sizeof(glm::mat4) * skinData_.size() * curFrame;
    skinBuffer_.copyData(skinData_.data(), sizeof(glm::mat4) * skinData_.size(), offset);
}