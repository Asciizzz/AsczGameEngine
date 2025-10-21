#include "TinyData/TinySkeleton.hpp"

using namespace TinyVK;

void TinySkeletonRT::init(TinyHandle skeletonHandle, const TinySkeleton& skeleton) {
    this->skeleHandle = skeletonHandle;
    this->skeleton = &skeleton;

    localPose.resize(skeleton.bones.size(), glm::mat4(1.0f));
    finalPose.resize(skeleton.bones.size(), glm::mat4(1.0f));
    skinData.resize(skeleton.bones.size(), glm::mat4(1.0f));

    // Initialize local pose to bind pose
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        localPose[i] = skeleton.bones[i].localBindTransform;
    }
}

void TinySkeletonRT::vkCreate(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descLayout) {
    // Create descriptor set
    descSet.allocate(deviceVK->device, descPool, descLayout);

    // Create skinning data buffer
    VkDeviceSize bufferSize = sizeof(glm::mat4) * skinData.size();
    skinBuffer.setDataSize(bufferSize)
              .setUsageFlags(BufferUsage::Storage)
              .setMemPropFlags(MemProp::HostVisibleAndCoherent)
              .createBuffer(deviceVK)
              .mapAndCopy(skinData.data());

    // Upload initial skin data
    skinBuffer.mapAndCopy(skinData.data());

    // Update descriptor set with skin buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = skinBuffer.get();
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    DescWrite()
        .setDstSet(descSet)
        .setType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSets(deviceVK->device);
}

void TinySkeletonRT::recursiveUpdate(uint32_t boneIndex, const glm::mat4& parentTransform) {
    const TinyBone& bone = skeleton->bones[boneIndex];

    finalPose[boneIndex] = parentTransform * localPose[boneIndex];
    skinData[boneIndex] = finalPose[boneIndex] * bone.inverseBindMatrix;

    for (int childIndex : bone.children) {
        recursiveUpdate(childIndex, finalPose[boneIndex]);
    }
}

void TinySkeletonRT::update() {
    recursiveUpdate(0, glm::mat4(1.0f));

    // Upload updated skin data to GPU
    skinBuffer.copyData(skinData.data());
}