#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Helpers/Templates.hpp"

namespace Az3D {
    struct Mesh;
    struct MeshMappingData;
    struct ModelInstance;
}

namespace AzVulk {

    struct BufferData {
        BufferData() = default;
        void initVulkan(VkDevice device, VkPhysicalDevice physicalDevice) {
            this->device = device;
            this->physicalDevice = physicalDevice;
        }

        ~BufferData() { cleanup(); }
        void cleanup();

        // Move constructor
        BufferData(BufferData&& other) noexcept;

        // Move assignment
        BufferData& operator=(BufferData&& other) noexcept;

        VkDevice device;
        VkPhysicalDevice physicalDevice;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        uint32_t resourceCount = 0;
        VkDeviceSize dataTypeSize = 0;
        VkDeviceSize totalDataSize = 0;

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        void createBuffer(
            size_t resourceCount, size_t dataTypeSize,
            VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
        );
        void recreateBuffer(size_t resourceCount);

        template<typename T>
        void uploadData(const std::vector<T>& data) {
            vkMapMemory(device, memory, 0, totalDataSize, 0, &mapped);
            memcpy(mapped, data.data(), sizeof(T) * data.size());
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
        template<typename T>
        void uploadData(const T* data) {
            vkMapMemory(device, memory, 0, totalDataSize, 0, &mapped);
            memcpy(mapped, data, sizeof(T) * resourceCount);
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }

        void mappedData() {
            if (!mapped) vkMapMemory(device, memory, 0, totalDataSize, 0, &mapped);
        }

        template<typename T>
        void mappedData(const std::vector<T>& data) {
            mappedData();
            memcpy(mapped, data.data(), sizeof(T) * data.size());
        }

        template<typename T>
        void updateMapped(size_t index, const T& value) {
            static_cast<T*>(mapped)[index] = value;
        }
    };

    class Buffer {
    public:
        Buffer(const class Device& device);
        ~Buffer();

        // Sharing buffers: not even once
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        // Soon to be legacy
        void createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances);
        void updateMeshInstanceBufferSelective( size_t meshIndex,
                                                Az3D::MeshMappingData& meshData, 
                                                const std::vector<Az3D::ModelInstance>& modelInstances);

        const Device& vulkanDevice;

        // Soon to be 6 feet under
        std::vector<BufferData> instanceBufferDatas;
    };
}
