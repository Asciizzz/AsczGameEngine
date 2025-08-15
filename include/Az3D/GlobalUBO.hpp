#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include "AzVulk/Buffer.hpp"
#include "AzVulk/DescriptorManager.hpp"
#include "AzVulk/DepthManager.hpp"

#include "Az3D/Camera.hpp"

namespace Az3D {

    struct GlobalUBO {
        // Camera matrices
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 view;

        // Remember to remove this in the future
        alignas(16) glm::vec4 cameraPos;     // xyz = camera position, w = fov (radians)
        alignas(16) glm::vec4 cameraForward; // xyz = camera forward, w = aspect ratio
        alignas(16) glm::vec4 cameraRight;   // xyz = camera right, w = unused
        alignas(16) glm::vec4 cameraUp;      // xyz = camera up, w = unused
        alignas(16) glm::vec4 nearFar;       // x = near, y = far, z = unused, w = unused
    };


    class GlobalUBOManager {
    public:
        GlobalUBOManager(
            VkDevice device, VkPhysicalDevice physicalDevice, size_t MAX_FRAMES_IN_FLIGHT,
            // Add additional global component as needed
            VkSampler depthSampler, VkImageView depthSamplerView

        ) : device(device), physicalDevice(physicalDevice), MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT),
            // Add additional global components as needed (1)
            depthSampler(depthSampler), depthSamplerView(depthSamplerView)

        {}

        ~GlobalUBOManager() { cleanup(); } void cleanup();

        // Delete copy constructor and assignment operator
        GlobalUBOManager(const GlobalUBOManager&) = delete;
        GlobalUBOManager& operator=(const GlobalUBOManager&) = delete;

        VkDevice device;
        VkPhysicalDevice physicalDevice;
        size_t MAX_FRAMES_IN_FLIGHT;

        // Global values
        GlobalUBO ubo;
        // Depth sampler
        
        VkSampler depthSampler = VK_NULL_HANDLE;
        VkImageView depthSamplerView = VK_NULL_HANDLE;

        std::vector<AzVulk::BufferData> bufferDatas;
        void createBufferDatas() {
            bufferDatas.resize(MAX_FRAMES_IN_FLIGHT);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                auto& bufferData = bufferDatas[i];
                bufferData.initVulkan(device, physicalDevice);

                bufferData.createBuffer(
                    1, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                );

                bufferData.mappedData();
            }
        }

        AzVulk::DynamicDescriptor dynamicDescriptor;
        void initDescriptorSets() {
            using namespace AzVulk;

            // Create layout
            dynamicDescriptor.init(device, MAX_FRAMES_IN_FLIGHT);
            dynamicDescriptor.createSetLayout({
                // Global UBO
                DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
                // Depth sampler
                DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                // ...
            });

            // Create pool
            dynamicDescriptor.createPool(1, {
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                // ...
            });
        }
    };

}