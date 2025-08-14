#pragma once

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include "Az3D/Az3D.hpp"

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

        void cleanup() {
            if (buffer != VK_NULL_HANDLE) {
                if (mapped) {
                    vkUnmapMemory(device, memory);
                    mapped = nullptr;
                }

                vkDestroyBuffer(device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
            }

            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }
        }

        VkDevice device;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;

        bool hasMapped = false;
        void* mapped = nullptr;

        VkDeviceSize size = 0;
        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        uint32_t resourceCount = 0;

        template<typename T>
        void createBuffer(
            const Device& vulkanDevice, const std::vector<T>& data,
            VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags,
            bool keepMapped=false
        ) {
            this->usageFlags = usageFlags;
            this->memoryFlags = memoryFlags;
            this->device = vulkanDevice.device;

            this->resourceCount = static_cast<uint32_t>(data.size());
            this->size = sizeof(T) * data.size();

            // Cleanup existing buffer if it exists
            cleanup();


            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usageFlags;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(vulkanDevice.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(vulkanDevice.device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = vulkanDevice.findMemoryType(memRequirements.memoryTypeBits, memoryFlags);

            if (vkAllocateMemory(vulkanDevice.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate buffer memory!");
            }

            vkBindBufferMemory(vulkanDevice.device, buffer, memory, 0);

            hasMapped = keepMapped;

            if (keepMapped) {
                // Memory mapping
                vkMapMemory(vulkanDevice.device, memory, 0, size, 0, &mapped);
                memcpy(mapped, data.data(), sizeof(T) * data.size());
            } else {
                vkMapMemory(vulkanDevice.device, memory, 0, size, 0, &mapped);
                memcpy(mapped, data.data(), sizeof(T) * data.size());
                vkUnmapMemory(vulkanDevice.device, memory);
                mapped = nullptr;
            }
        }
    };

    // Multi-mesh data structure for storing multiple mesh types
    struct MeshBufferData {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        
        // Instance buffer for this specific mesh type
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
        void* instanceBufferMapped = nullptr;
        // Note: instanceCount removed - now tracked per-ModelGroup
        
        // Cleanup helper - destructors aren't enough apparently
        void cleanup(VkDevice device) {
            if (instanceBufferMapped) {
                vkUnmapMemory(device, instanceBufferMemory);
            }
            if (instanceBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, instanceBuffer, nullptr);
                vkFreeMemory(device, instanceBufferMemory, nullptr);
            }
            if (indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, indexBuffer, nullptr);
                vkFreeMemory(device, indexBufferMemory, nullptr);
            }
            if (vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, vertexBuffer, nullptr);
                vkFreeMemory(device, vertexBufferMemory, nullptr);
            }
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
        
        // Material uniform buffer methods
        void createMaterialUniformBuffers(const std::vector<Az3D::Material>& materials);
        void updateMaterialUniformBuffer(size_t materialIndex, const Az3D::Material& material);
        VkBuffer getMaterialUniformBuffer(size_t materialIndex) const;

        size_t loadMeshToBuffer(const Az3D::Mesh& mesh);  // Returns mesh index
        // Most efficient versions that work directly with mesh mapping data
        void createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances);
        // Selective update method - only updates specific instances based on update queue
        void updateMeshInstanceBufferSelective( size_t meshIndex,
                                                Az3D::MeshMappingData& meshData, 
                                                const std::vector<Az3D::ModelInstance>& modelInstances);

        const Device& vulkanDevice;
        
        // Multi-mesh buffers storage
        std::vector<MeshBufferData> meshBuffers;

        // Uniform buffer arrays
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;
        
        // Material uniform buffers
        std::vector<VkBuffer> materialUniformBuffers;
        std::vector<VkDeviceMemory> materialUniformBuffersMemory;
        std::vector<void*> materialUniformBuffersMapped;
    };
}
