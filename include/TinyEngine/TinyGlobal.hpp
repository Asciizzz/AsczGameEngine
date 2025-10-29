#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"


class tinyCamera;

class tinyGlobal {
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

    tinyGlobal(uint32_t maxFramesInFlight=2)
    : maxFramesInFlight(maxFramesInFlight) {}

    // Delete copy constructor and assignment operator
    tinyGlobal(const tinyGlobal&) = delete;
    tinyGlobal& operator=(const tinyGlobal&) = delete;

    uint32_t maxFramesInFlight = 2;

    void update(const tinyCamera& camera, uint32_t frameIndex);

    VkDescriptorSetLayout getDescLayout() const { return descLayout; }
    VkDescriptorSet getDescSet() const { return descSet; }

    tinyVk::DescLayout descLayout;
    tinyVk::DescPool descPool;

    // New design! Use only 1 buffer with dynamic offsets
    size_t alignedSize = 0;
    tinyVk::DataBuffer dataBuffer;
    tinyVk::DescSet descSet;

    void vkCreate(const tinyVk::Device* deviceVk);
};