#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorSets.hpp"

namespace Az3D {
    class Camera;

    struct GlobalUBO {
        // Camera matrices
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 view;

        alignas(16) glm::vec4 prop1; // General purpose: <float time>, <unused>, <unused>, <unused>

        // Remember to remove this in the future
        alignas(16) glm::vec4 cameraPos;     // xyz = camera position, w = fov (radians)
        alignas(16) glm::vec4 cameraForward; // xyz = camera forward, w = aspect ratio
        alignas(16) glm::vec4 cameraRight;   // xyz = camera right, w = near
        alignas(16) glm::vec4 cameraUp;      // xyz = camera up, w = far
    };


    class GlobalUBOManager {
    public:
        GlobalUBOManager(
            VkDevice device, VkPhysicalDevice physicalDevice, uint32_t MAX_FRAMES_IN_FLIGHT,
            // Add additional global component as needed
            VkSampler depthSampler, VkImageView depthSamplerView
        );
        ~GlobalUBOManager() = default;

        // Delete copy constructor and assignment operator
        GlobalUBOManager(const GlobalUBOManager&) = delete;
        GlobalUBOManager& operator=(const GlobalUBOManager&) = delete;

        VkDevice device;
        VkPhysicalDevice physicalDevice;
        uint32_t MAX_FRAMES_IN_FLIGHT;

        GlobalUBO ubo;
        // Depth sampler
        VkSampler depthSampler = VK_NULL_HANDLE;
        VkImageView depthSamplerView = VK_NULL_HANDLE;

        std::vector<AzVulk::BufferData> bufferDatas;
        void createBufferDatas();

        AzVulk::DynamicDescriptor dynamicDescriptor;
        void initDescriptorSets();
        void createDescriptorSets();
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex);

    // Functionalities
        void updateUBO(const Camera& camera);
        void resizeWindow(VkSampler newDepthSampler, VkImageView newDepthSamplerView);

    };

}