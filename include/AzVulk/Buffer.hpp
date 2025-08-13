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

    // Most commonly used ones
    enum BufferType {
        None           = 0,
        VertexBit      = 1 << 0,
        IndexBit       = 1 << 1,
        UniformBit     = 1 << 2,
        StorageBit     = 1 << 3,
        TransferSrcBit = 1 << 4,
        TransferDstBit = 1 << 5
    };
    inline VkBufferUsageFlags getBufferUsageFlags(int type) {
        VkBufferUsageFlags flags = 0;
        if (type & VertexBit)      flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (type & IndexBit)       flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (type & UniformBit)     flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (type & StorageBit)     flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (type & TransferSrcBit) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (type & TransferDstBit) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return flags;
    }

    struct BufferData {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;

        VkDevice device = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        VkMemoryPropertyFlags memoryFlags = 0;

        uint32_t resourceNum = 0;

        BufferData( VkDevice device, VkMemoryAllocateInfo allocInfo,
                    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
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

        // Helper methods 
        void createBuffer(  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);
        // === Dummy Vertex Buffer Support ===
        VkBuffer dummyVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory dummyVertexBufferMemory = VK_NULL_HANDLE;

        // Dummy vertex buffer creation and access (for passes with no real vertex input)
        void createDummyVertexBuffer();
        void destroyDummyVertexBuffer();
    };
}
