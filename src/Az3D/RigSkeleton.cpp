#include "Az3D/RigSkeleton.hpp"
#include <iostream>

using namespace Az3D;


void RigSkeleton::debugPrintHierarchy() const {
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == -1) {
            debugPrintRecursive(static_cast<int>(i), 0);
        }
    }
}

void RigSkeleton::debugPrintRecursive(int boneIndex, int depth) const {
    // Indentation
    for (int i = 0; i < depth; ++i) std::cout << "| ";

    // Print this bone
    std::cout << "| " << names[boneIndex] << " (index " << boneIndex << ")";
    if (parentIndices[boneIndex] != -1) {
        std::cout << " [parent " << parentIndices[boneIndex] << "]";
    }
    std::cout << "\n";

    // Recurse into children
    for (size_t i = 0; i < names.size(); ++i) {
        if (parentIndices[i] == boneIndex) {
            debugPrintRecursive(static_cast<int>(i), depth + 1);
        }
    }
}


void RigDemo::computeAllTransforms() {
    for (size_t i = 0; i < rigSkeleton->names.size(); ++i) {
        int parent = rigSkeleton->parentIndices[i];
        if (parent == -1) {
            globalPoseTransforms[i] = localPoseTransforms[i];
        } else {
            globalPoseTransforms[i] = globalPoseTransforms[parent] * localPoseTransforms[i];
        }
    }

    // Compute final transforms
    for (size_t i = 0; i < rigSkeleton->names.size(); ++i) {
        finalTransforms[i] = globalPoseTransforms[i] * rigSkeleton->inverseBindMatrices[i];
    }
}

void RigDemo::init(const AzVulk::Device* vkDevice, const SharedPtr<RigSkeleton>& skeleton) {
    using namespace AzVulk;

    rigSkeleton = skeleton;

    skeleton->debugPrintHierarchy();

    localPoseTransforms.resize(skeleton->names.size());
    globalPoseTransforms.resize(skeleton->names.size());
    finalTransforms.resize(skeleton->names.size());
    
    localPoseTransforms = skeleton->localBindTransforms;

    computeAllTransforms();

    finalPoseBuffer.initVkDevice(vkDevice);

    finalPoseBuffer.setProperties(
        finalTransforms.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    finalPoseBuffer.createBuffer();
    finalPoseBuffer.mapMemory();
    finalPoseBuffer.copyData(finalTransforms.data());


    descLayout.init(vkDevice->lDevice);
    descLayout.create({
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    descPool.init(vkDevice->lDevice);
    descPool.create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

    descSet.init(vkDevice->lDevice);
    descSet.allocate(descPool.get(), descLayout.get(), 1);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = finalPoseBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glm::mat4) * finalTransforms.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet.get();
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(vkDevice->lDevice, 1, &write, 0, nullptr);

    update();
}

void RigDemo::update() {
    finalPoseBuffer.copyData(finalTransforms.data());
}