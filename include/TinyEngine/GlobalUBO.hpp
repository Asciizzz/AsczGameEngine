#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "TinyVK/DataBuffer.hpp"
#include "TinyVK/Descriptor.hpp"

class TinyCamera;

namespace TinyEngine {

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
    GlbUBOManager(const TinyVK::Device* deviceVK, uint32_t maxFramesInFlight=2);
    ~GlbUBOManager() = default;

    // Delete copy constructor and assignment operator
    GlbUBOManager(const GlbUBOManager&) = delete;
    GlbUBOManager& operator=(const GlbUBOManager&) = delete;

    const TinyVK::Device* deviceVK;

    GlobalUBO ubo;

    uint32_t maxFramesInFlight = 2; // Must match Renderer
    std::vector<TinyVK::DataBuffer> dataBuffer; // Per-frame buffers
    void createDataBuffer();

    TinyVK::DescLayout descLayout;
    TinyVK::DescPool descPool;
    std::vector<TinyVK::DescSet> descSets;
    void createDescSets();

    VkDescriptorSet getDescSet(uint32_t frameIndex) const { return descSets[frameIndex]; }
    VkDescriptorSetLayout getDescLayout() const { return descLayout; }

// Functionalities
    void updateUBO(const TinyCamera& camera, uint32_t frameIndex);
};

}