#include "TinyData/TinySkeleton.hpp"

using namespace TinyVK;

void TinySkeletonRT::set(const TinySkeleton& skeleton) {
    boneNames.clear();
    boneParents.clear();
    boneChildren.clear();
    boneInverseBindMatrices.clear();
    boneLocalBindTransforms.clear();

    for (const auto& bone :skeleton.bones) {
        boneNames.push_back(bone.name);
        boneParents.push_back(bone.parent);
        boneChildren.push_back(bone.children);
        boneInverseBindMatrices.push_back(bone.inverseBindMatrix);
        boneLocalBindTransforms.push_back(bone.localBindTransform);

        // Default final transform to identity
        boneTransformsFinal.push_back(glm::mat4(1.0f));
    }
}

void TinySkeletonRT::vkCreate(const TinyVK::Device* deviceVK, VkDescriptorPool descPool, VkDescriptorSetLayout descSetLayout) {
    // Create GPU buffer for final bone transforms
    VkDeviceSize bufferSize = sizeof(glm::mat4) * boneTransformsFinal.size();

    // Bone buffer is host visible for easy updates
    boneFinalBuffer
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Uniform)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(boneTransformsFinal.data());

    // Create descriptor set for skinning shader
    boneDescSet.allocate(deviceVK->device, descPool, descSetLayout);

    VkDescriptorBufferInfo boneBufferInfo{};
    boneBufferInfo.buffer = boneFinalBuffer; // Implicit conversion
    boneBufferInfo.offset = 0;
    boneBufferInfo.range = bufferSize;

    DescWrite()
        .setDstSet(boneDescSet)
        .setType(DescType::UniformBuffer)
        .setDescCount(1)
        .setBufferInfo({boneBufferInfo})
        .updateDescSets(deviceVK->device);
}