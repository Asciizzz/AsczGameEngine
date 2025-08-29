#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

class Camera;

struct GlobalUBO {
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
    GlobalUBOManager(const AzVulk::Device* vkDevice);
    ~GlobalUBOManager() = default;

    // Delete copy constructor and assignment operator
    GlobalUBOManager(const GlobalUBOManager&) = delete;
    GlobalUBOManager& operator=(const GlobalUBOManager&) = delete;

    const AzVulk::Device* vkDevice;

    GlobalUBO ubo;

    AzVulk::BufferData bufferData;
    void createBufferData();

    AzVulk::DescLayout descLayout;
    AzVulk::DescPool descPool;
    AzVulk::DescSets descSet; // Only need 1 set for global UBO

    void createDescLayout();
    void createDescPool();
    void createDescSet();

    VkDescriptorSet getDescriptorSet();

// Functionalities
    void updateUBO(const Camera& camera);
};

}