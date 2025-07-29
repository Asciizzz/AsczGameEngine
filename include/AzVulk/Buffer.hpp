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
    };

    // Instance data structure for instanced rendering
    struct ModelInstance {
        alignas(16) glm::mat4 modelMatrix;
        
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();
    };

    // Billboard instance data structure for billboard rendering
    struct BillboardInstance {
        alignas(16) glm::vec3 position;
        alignas(4) float width;
        alignas(4) float height;
        alignas(4) uint32_t textureIndex;
        alignas(8) glm::vec2 uvMin;  // AB1 - top-left UV
        alignas(8) glm::vec2 uvMax;  // AB2 - bottom-right UV
        
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions();
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
        uint32_t instanceCount = 0;
        
        // Cleanup helper
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

        // Delete copy constructor and assignment operator
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        // Legacy single-mesh methods (for backwards compatibility)
        void createVertexBuffer(const std::vector<Az3D::Vertex>& vertices);
        void createIndexBuffer(const std::vector<uint16_t>& indices);
        void createIndexBuffer(const std::vector<uint32_t>& indices);
        void createUniformBuffers(size_t count);
        void loadMesh(const Az3D::Mesh& mesh);
        void createVertexBuffer(const Az3D::Mesh& mesh);
        void createInstanceBuffer(const std::vector<ModelInstance>& instances);
        void updateInstanceBuffer(const std::vector<ModelInstance>& instances);
        
        // New multi-mesh methods
        size_t loadMeshToBuffer(const Az3D::Mesh& mesh);  // Returns mesh index
        void createInstanceBufferForMesh(size_t meshIndex, const std::vector<ModelInstance>& instances);
        void updateInstanceBufferForMesh(size_t meshIndex, const std::vector<ModelInstance>& instances);
        
        // Billboard buffer methods
        void createBillboardInstanceBuffer(const std::vector<BillboardInstance>& instances);
        void updateBillboardInstanceBuffer(const std::vector<BillboardInstance>& instances);
        
        // Getters for multi-mesh data
        const std::vector<MeshBufferData>& getMeshBuffers() const { return meshBuffers; }
        size_t getMeshCount() const { return meshBuffers.size(); }

        
        const Device& vulkanDevice;
        
        // Multi-mesh buffers storage
        std::vector<MeshBufferData> meshBuffers;
        
        // Legacy single-mesh buffers (for backwards compatibility)
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        VkIndexType indexType = VK_INDEX_TYPE_UINT16;
        
        // Legacy instance buffer for backwards compatibility
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
        void* instanceBufferMapped = nullptr;
        uint32_t instanceCount = 0;
        
        // Billboard instance buffer
        VkBuffer billboardInstanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory billboardInstanceBufferMemory = VK_NULL_HANDLE;
        void* billboardInstanceBufferMapped = nullptr;
        uint32_t billboardInstanceCount = 0;
        
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        // Helper methods (now public for direct access)
        void createBuffer(  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);
    };
}
