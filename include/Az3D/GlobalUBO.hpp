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


class GlbUBOManager {
public:
    GlbUBOManager(const AzVulk::Device* vkDevice);
    ~GlbUBOManager() = default;

    // Delete copy constructor and assignment operator
    GlbUBOManager(const GlbUBOManager&) = delete;
    GlbUBOManager& operator=(const GlbUBOManager&) = delete;

    const AzVulk::Device* vkDevice;

    GlobalUBO ubo;

    static const int MAX_FRAMES_IN_FLIGHT = 2; // Must match Renderer
    std::vector<AzVulk::BufferData> bufferData; // Per-frame buffers
    void createBufferData();

    AzVulk::DescLayout descLayout;
    AzVulk::DescPool descPool;
    std::vector<AzVulk::DescSets> descSets; // Per-frame descriptor sets

    void createDescLayout();
    void createDescPool();
    void createDescSet();

    VkDescriptorSet getDescSet(uint32_t frameIndex) const { return descSets[frameIndex].get(); }
    VkDescriptorSetLayout getDescLayout() const { return descLayout.get(); }

// Functionalities
    void updateUBO(const Camera& camera, uint32_t frameIndex);
};

}