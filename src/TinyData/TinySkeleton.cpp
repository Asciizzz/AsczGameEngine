#include "TinyData/TinySkeletonRT.hpp"

using namespace TinyVK;

// ========================== TINY SKELETON RT ==========================

void TinySkeletonRT::init(const TinyVK::Device* deviceVK, const TinyRegistry* fsRegistry, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout) {
    vkValid = true;

    deviceVK_ = deviceVK;
    fsRegistry_ = fsRegistry;

    descSet_.allocate(deviceVK->device, descPool, descLayout);
}

void TinySkeletonRT::set(TinyHandle skeletonHandle) {
    skeleHandle_ = skeletonHandle;

    const TinySkeleton* skeleton = rSkeleton();
    if (!vkValid || skeleton == nullptr) return;

    localPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
    finalPose_.resize(skeleton->bones.size(), glm::mat4(1.0f));
    skinData_.resize(skeleton->bones.size(), glm::mat4(1.0f));

    // Initialize local pose to bind pose
    for (size_t i = 0; i < skeleton->bones.size(); ++i) {
        localPose_[i] = skeleton->bones[i].localBindTransform;
    }

    vkCreate();
    update();
}

void TinySkeletonRT::copy(const TinySkeletonRT* other) {
    if (!other || !other->pValid()) return;

    skeleHandle_ = other->skeleHandle_; // Guaranteed to be valid since other is valid

    localPose_ = other->localPose_;
    finalPose_ = other->finalPose_;
    skinData_ = other->skinData_;

    vkCreate();
    update();
}

void TinySkeletonRT::vkCreate() {
    if (!hasSkeleton()) return;

    // Create skinning data buffer
    VkDeviceSize bufferSize = sizeof(glm::mat4) * skinData_.size();
    skinBuffer_
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK_)
        .mapAndCopy(skinData_.data());

    // Update descriptor set with skin buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = skinBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    DescWrite()
        .setDstSet(descSet_)
        .setType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSets(deviceVK_->device);
}

void TinySkeletonRT::refresh(uint32_t boneIndex, bool reupdate) {
    // Reinitialize local pose of specific bone to bind pose
    if (boneIndex >= rSkeleton()->bones.size()) return;

    localPose_[boneIndex] = rSkeleton()->bones[boneIndex].localBindTransform;
    if (reupdate) update(boneIndex);
}

void TinySkeletonRT::refreshAll() {
    for (size_t i = 0; i < rSkeleton()->bones.size(); ++i) {
        localPose_[i] = rSkeleton()->bones[i].localBindTransform;
    }
    updateFlat();
}

void TinySkeletonRT::updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform) {
    if (boneIndex >= rSkeleton()->bones.size()) return;

    const TinyBone& bone = rSkeleton()->bones[boneIndex];

    finalPose_[boneIndex] = parentTransform * localPose_[boneIndex];
    skinData_[boneIndex] = finalPose_[boneIndex] * bone.inverseBindMatrix;

    for (int childIndex : bone.children) {
        updateRecursive(childIndex, finalPose_[boneIndex]);
    }
}

void TinySkeletonRT::updateFlat() {
    for (size_t i = 0; i < rSkeleton()->bones.size(); ++i) {
        const TinyBone& bone = rSkeleton()->bones[i];

        glm::mat4 parentTransform = glm::mat4(1.0f);
        if (bone.parent != -1) {
            parentTransform = finalPose_[bone.parent];
        }

        finalPose_[i] = parentTransform * localPose_[i];
        skinData_[i] = finalPose_[i] * bone.inverseBindMatrix;
    }
}

void TinySkeletonRT::update(uint32_t index) {
    if (!boneValid(index)) return;

    if (index == 0) {
        updateFlat();
    } else {
        // Retrieve parent
        glm::mat4 parentTransform = finalPose_[rSkeleton()->bones[index].parent];
        updateRecursive(index, parentTransform);
    }

    // Upload updated skin data to GPU
    skinBuffer_.copyData(skinData_.data());
}