#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorSets.hpp"

namespace Az3D {

class Camera;

struct alignas(16) GlobalUBO {
    // Camera matrices
    glm::mat4 proj;
    glm::mat4 view;

    glm::vec4 prop1; // General purpose: <float time>, <unused>, <unused>, <unused>

    // Remember to remove this in the future
    glm::vec4 cameraPos;     // xyz = camera position, w = fov (radians)
    glm::vec4 cameraForward; // xyz = camera forward, w = aspect ratio
    glm::vec4 cameraRight;   // xyz = camera right, w = near
    glm::vec4 cameraUp;      // xyz = camera up, w = far
};


class GlobalUBOManager {
public:
    GlobalUBOManager(const AzVulk::Device* vkDevice, uint32_t MAX_FRAMES_IN_FLIGHT);
    ~GlobalUBOManager() = default;

    // Delete copy constructor and assignment operator
    GlobalUBOManager(const GlobalUBOManager&) = delete;
    GlobalUBOManager& operator=(const GlobalUBOManager&) = delete;

    const AzVulk::Device* vkDevice;
    uint32_t MAX_FRAMES_IN_FLIGHT;

    GlobalUBO ubo;

    std::vector<AzVulk::BufferData> bufferDatas;
    void createBufferDatas();

    AzVulk::DynamicDescriptor dynamicDescriptor;
    void initDescriptorSets();
    void createDescriptorSets();
    VkDescriptorSet getDescriptorSet(uint32_t frameIndex);

// Functionalities
    void updateUBO(const Camera& camera);
};

}