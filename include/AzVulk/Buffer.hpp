#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <vector>
#include <array>

#include "Az3D/Az3D.hpp"
#include "AzVulk/Device.hpp"

namespace AzVulk {
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

    // Material properties for per-material uniform buffer
    struct MaterialUBO {
        alignas(16) glm::vec4 prop1;

        MaterialUBO() : prop1(1.0f, 0.0f, 0.0f, 0.0f) {}
        MaterialUBO(const glm::vec4& p1) : prop1(p1) {}
    };


    struct BufferData {
        enum Type {
            None = 0,
            Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        enum Usage {
            DeviceLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            HostCached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            LazilyAllocated = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
        };


        BufferData() = default;
        ~BufferData() { cleanup(); }
        void cleanup();

        // Non-copyable
        BufferData(const BufferData&) = delete;
        BufferData& operator=(const BufferData&) = delete;

        // Move constructor
        BufferData(BufferData&& other) noexcept;

        // Move assignment
        BufferData& operator=(BufferData&& other) noexcept;

        VkDevice device;
        VkPhysicalDevice physicalDevice;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        VkDeviceSize dataSize = 0;
        uint32_t resourceCount = 0;

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        void createBuffer(
            const Device& vulkanDevice, size_t dataTypeSize, size_t resourceCount,
            VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
        );

        template<typename T>
        void uploadData(const std::vector<T>& data) {
            if (sizeof(T) * data.size() != dataSize) {
                throw std::runtime_error("Data type size mismatch!");
            }

            vkMapMemory(device, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data.data(), sizeof(T) * data.size());
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
        template<typename T>
        void uploadData(const T* data) {
            if (sizeof(T) * resourceCount != dataSize) {
                throw std::runtime_error("Data type size mismatch!");
            }

            vkMapMemory(device, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data, sizeof(T) * resourceCount);
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }

        template<typename T>
        void mapData(const std::vector<T>& data) {
            vkMapMemory(device, memory, 0, dataSize, 0, &mapped);
            memcpy(mapped, data.data(), sizeof(T) * data.size());
        }
    };

    // Multi-mesh data structure for storing multiple mesh types
    struct MeshBufferData {
        BufferData vertexBufferData;
        BufferData indexBufferData;

        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
        void* instanceBufferMapped = nullptr;

        BufferData instanceBufferData;

        // Cleanup helper - destructors aren't enough apparently
        void cleanup(VkDevice device) {
            if (instanceBufferMapped) {
                vkUnmapMemory(device, instanceBufferMemory);
            }
            if (instanceBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, instanceBuffer, nullptr);
                vkFreeMemory(device, instanceBufferMemory, nullptr);
            }

            instanceBufferData.cleanup();
            vertexBufferData.cleanup();
            indexBufferData.cleanup();
        }
    };

    class Buffer {
    public:
        Buffer(const class Device& device);
        ~Buffer();

        // Sharing buffers: not even once
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        void createUniformBuffers(size_t count);

        void creatematerialBuffers(const std::vector<Az3D::Material>& materials);

        size_t createMeshBuffer(const Az3D::Mesh& mesh);  // Returns mesh index
        // Most efficient versions that work directly with mesh mapping data
        void createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances);
        // Selective update method - only updates specific instances based on update queue
        void updateMeshInstanceBufferSelective( size_t meshIndex,
                                                Az3D::MeshMappingData& meshData, 
                                                const std::vector<Az3D::ModelInstance>& modelInstances);

        const Device& vulkanDevice;

        std::vector<BufferData> uniformBuffers;
        std::vector<MeshBufferData> meshBuffers;
        std::vector<BufferData> materialBuffers;
    };
}
