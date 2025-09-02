#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

#include <iostream>

namespace Az3D {

struct RigSkeleton {
    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    void debugPrintHierarchy() const;

private:
    void debugPrintRecursive(int boneIndex, int depth) const;
};

// I am way too tired
// Don't forget to delete this btw
struct RigDemo {
    SharedPtr<RigSkeleton> rigSkeleton;

    std::vector<glm::mat4> localPoseTransforms; // <- User changeable

    std::vector<glm::mat4> globalPoseTransforms; // <--DO NOT CHANGE-- Recursive transforms

    std::vector<glm::mat4> finalTransforms; // <--DO NOT CHANGE-- GPU eat this up!

    AzVulk::BufferData finalPoseBuffer;
    AzVulk::DescLayout descLayout;
    AzVulk::DescPool   descPool;
    AzVulk::DescSets   descSet;

    size_t meshIndex = 0; // Which mesh to apply this rig to

    void computeTransforms() {
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

    void init(const AzVulk::Device* vkDevice, const SharedPtr<RigSkeleton>& skeleton) {
        using namespace AzVulk;

        rigSkeleton = skeleton;

        skeleton->debugPrintHierarchy();

        localPoseTransforms.resize(skeleton->names.size());
        globalPoseTransforms.resize(skeleton->names.size());
        finalTransforms.resize(skeleton->names.size());

        for (size_t i = 0; i < skeleton->names.size(); ++i) {
            localPoseTransforms[i] = skeleton->localBindTransforms[i];
        }

        // Rotate the root
        localPoseTransforms[0] = glm::rotate(localPoseTransforms[0], glm::radians(-90.0f), glm::vec3(0,1,0));

        computeTransforms();

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

    void update() {
        finalPoseBuffer.copyData(finalTransforms.data());
    }
};

} // namespace Az3D
