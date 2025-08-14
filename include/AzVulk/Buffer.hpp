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

        // Non-copyable
        BufferData(const BufferData&) = delete;
        BufferData& operator=(const BufferData&) = delete;

        // Move constructor
        BufferData(BufferData&& other) noexcept {
            device = other.device;
            physicalDevice = other.physicalDevice;

            buffer = other.buffer;
            memory = other.memory;
            mapped = other.mapped;

            dataTypeSize = other.dataTypeSize;
            resourceCount = other.resourceCount;
            totalSize = other.totalSize;

            usageFlags = other.usageFlags;
            memoryFlags = other.memoryFlags;

            // Invalidate other's handles
            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.mapped = nullptr;
        }

        // Move assignment
        BufferData& operator=(BufferData&& other) noexcept {
            if (this != &other) {
                cleanup();
                device = other.device;
                physicalDevice = other.physicalDevice;
                
                buffer = other.buffer;
                memory = other.memory;
                mapped = other.mapped;

                dataTypeSize = other.dataTypeSize;
                resourceCount = other.resourceCount;
                totalSize = other.totalSize;

                usageFlags = other.usageFlags;
                memoryFlags = other.memoryFlags;

                // Invalidate other's handles
                other.buffer = VK_NULL_HANDLE;
                other.memory = VK_NULL_HANDLE;
                other.mapped = nullptr;
            }
            return *this;
        }

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
        VkPhysicalDevice physicalDevice;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        uint32_t resourceCount = 0;
        VkDeviceSize dataTypeSize = 0;
        VkDeviceSize totalSize = 0;

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        void createBuffer(
            const Device& vulkanDevice, size_t dataTypeSize, size_t resourceCount,
            VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
        ) {
            this->usageFlags = usageFlags;
            this->memoryFlags = memoryFlags;
            this->device = vulkanDevice.device;
            this->physicalDevice = vulkanDevice.physicalDevice;

            this->resourceCount = static_cast<uint32_t>(resourceCount);
            this->dataTypeSize = static_cast<VkDeviceSize>(dataTypeSize);
            this->totalSize = dataTypeSize * resourceCount;

            cleanup();

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = totalSize;
            bufferInfo.usage = usageFlags;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, memoryFlags, physicalDevice);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate buffer memory!");
            }

            vkBindBufferMemory(device, buffer, memory, 0);
        }

        template<typename T>
        void uploadData(const std::vector<T>& data) {
            if (sizeof(T) != dataTypeSize) {
                throw std::runtime_error("Data type size mismatch!");
            }

            vkMapMemory(device, memory, 0, totalSize, 0, &mapped);
            memcpy(mapped, data.data(), sizeof(T) * data.size());
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }

        template<typename T>
        void mapData(const std::vector<T>& data) {
            vkMapMemory(device, memory, 0, totalSize, 0, &mapped);
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
