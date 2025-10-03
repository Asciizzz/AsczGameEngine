#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"


class TinyCamera;

class TinyGlobal {
public:
    struct UBO {
        // Camera matrices
        glm::mat4 proj;
        glm::mat4 view;

        glm::vec4 prop1; // General purpose: <float time>, <unused>, <unused>, <unused>

        glm::vec4 cameraPos;     // xyz = camera position, w = fov (radians)
        glm::vec4 cameraForward; // xyz = camera forward, w = aspect ratio
        glm::vec4 cameraRight;   // xyz = camera right, w = near
        glm::vec4 cameraUp;      // xyz = camera up, w = far
    } ubo;

    TinyGlobal(uint32_t maxFramesInFlight=2)
    : maxFramesInFlight(maxFramesInFlight) {}

    // Delete copy constructor and assignment operator
    TinyGlobal(const TinyGlobal&) = delete;
    TinyGlobal& operator=(const TinyGlobal&) = delete;

    uint32_t maxFramesInFlight = 2;

    void update(const TinyCamera& camera, uint32_t frameIndex);

    VkDescriptorSet getDescSet(uint32_t frameIndex) const { return descSets[frameIndex]; }
    VkDescriptorSetLayout getDescLayout() const { return descLayout; }

    std::vector<TinyVK::DataBuffer> dataBuffer; // Per-frame buffers
    void createDataBuffer(const TinyVK::Device* deviceVK);

    TinyVK::DescLayout descLayout;
    TinyVK::DescPool descPool;
    std::vector<TinyVK::DescSet> descSets;
    void createDescResources(const TinyVK::Device* deviceVK);

    void createVkResources(const TinyVK::Device* deviceVK);

};