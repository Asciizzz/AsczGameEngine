#include "TinyData/TinySkeleton.hpp"

using namespace TinyVK;

// ========================== TINY SKELETON RT ==========================


TinySkeletonRT::TinySkeletonRT(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout)
: vkValid(true) {
    this->deviceVK = deviceVK;

    descSet_.allocate(deviceVK->device, descPool, descLayout);
}

void TinySkeletonRT::set(TinyHandle skeletonHandle, const TinySkeleton* skeleton) {
    if (!vkValid || skeleton == nullptr) return;

    this->skeleHandle = skeletonHandle;
    this->skeleton = skeleton;

    localPose.resize(skeleton->bones.size(), glm::mat4(1.0f));
    finalPose.resize(skeleton->bones.size(), glm::mat4(1.0f));
    skinData.resize(skeleton->bones.size(), glm::mat4(1.0f));

    // Initialize local pose to bind pose
    for (size_t i = 0; i < skeleton->bones.size(); ++i) {
        localPose[i] = skeleton->bones[i].localBindTransform;
    }

    vkCreate();
    update();
}

void TinySkeletonRT::copy(const TinySkeletonRT& other) {
    if (!vkValid || !other.vkValid) return;

    skeleHandle = other.skeleHandle;
    skeleton = other.skeleton;

    localPose = other.localPose;
    finalPose = other.finalPose;
    skinData = other.skinData;
    printf("Copied TinySkeletonRT with %zu bones.\n", localPose.size());

    vkCreate();
    update();
}

void TinySkeletonRT::vkCreate() {
    if (!hasSkeleton()) return;

    // Create skinning data buffer
    VkDeviceSize bufferSize = sizeof(glm::mat4) * skinData.size();
    skinBuffer_
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(skinData.data());

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
        .updateDescSets(deviceVK->device);

    printf("Vulkan resources created for TinySkeletonRT with %zu bones.\n", skeleton->bones.size());
}

void TinySkeletonRT::refresh(uint32_t boneIndex, bool reupdate) {
    // Reinitialize local pose of specific bone to bind pose
    if (boneIndex >= skeleton->bones.size()) return;

    localPose[boneIndex] = skeleton->bones[boneIndex].localBindTransform;
    if (reupdate) update();
}

void TinySkeletonRT::refreshAll() {
    // Reinitialize all local poses to bind poses
    for (size_t i = 0; i < skeleton->bones.size(); ++i) {
        localPose[i] = skeleton->bones[i].localBindTransform;
    }

    update();
}

void TinySkeletonRT::updateRecursive(uint32_t boneIndex, const glm::mat4& parentTransform) {
    if (boneIndex >= skeleton->bones.size()) return;

    const TinyBone& bone = skeleton->bones[boneIndex];

    finalPose[boneIndex] = parentTransform * localPose[boneIndex];
    skinData[boneIndex] = finalPose[boneIndex] * bone.inverseBindMatrix;

    for (int childIndex : bone.children) {
        updateRecursive(childIndex, finalPose[boneIndex]);
    }
}

void TinySkeletonRT::update() {
    if (!hasSkeleton()) return;

    updateRecursive(0, glm::mat4(1.0f));

    // Upload updated skin data to GPU
    skinBuffer_.copyData(skinData.data());
}