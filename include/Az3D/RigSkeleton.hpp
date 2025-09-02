#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

struct RigSkeleton {
    // Bone SoA
    std::vector<std::string> names;
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> localBindTransforms;

    std::unordered_map<std::string, int> nameToIndex;

    void debugPrintHierarchy() const;
    void debugPrintRecursive(int boneIndex, int depth) const;
};

// I am way too tired
// Don't forget to delete this btw
struct RigDemo {
    SharedPtr<RigSkeleton> rigSkeleton;

    std::vector<glm::mat4> localPoseTransforms; // User changeable
    std::vector<glm::mat4> globalPoseTransforms; // Final result
    void computeGlobalTransforms();

    AzVulk::BufferData globalPoseBuffer;

    void init(const AzVulk::Device* vkDevice, const SharedPtr<RigSkeleton>& skeleton) {
        using namespace AzVulk;
        
        rigSkeleton = skeleton;
        globalPoseBuffer.initVkDevice(vkDevice);

        globalPoseBuffer.setProperties(
            globalPoseTransforms.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        globalPoseBuffer.createBuffer();
        globalPoseBuffer.mapMemory();


        descLayout.init(vkDevice->lDevice);
        descLayout.create({
            DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
        });

        descPool.init(vkDevice->lDevice);
        descPool.create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

        descSet.init(vkDevice->lDevice);
        descSet.allocate(descPool.get(), descLayout.get(), 1);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = globalPoseBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(glm::mat4) * globalPoseTransforms.size();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet.get();
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(vkDevice->lDevice, 1, &write, 0, nullptr);
    }

    void update() {
        globalPoseBuffer.copyData(globalPoseTransforms.data());
    }

    AzVulk::DescLayout descLayout;
    AzVulk::DescPool   descPool;
    AzVulk::DescSets   descSet;

};

} // namespace Az3D
