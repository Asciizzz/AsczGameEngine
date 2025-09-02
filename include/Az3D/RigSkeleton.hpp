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

    void computeBone(const std::string& boneName);
    void computeBone(size_t boneIndex);

    void computeAllTransforms();

    void init(const AzVulk::Device* vkDevice, const SharedPtr<RigSkeleton>& skeleton);
    void updateBuffer();

    float funAccumTimeValue = 0.0f;

    void funFunction(float dTime);

};

} // namespace Az3D
