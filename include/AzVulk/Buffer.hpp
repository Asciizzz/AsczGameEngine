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
    // Legacy Vertex struct for compatibility - will be converted from Az3D::Vertex
    struct Vertex {
        glm::vec3 p;
        glm::vec3 n;
        glm::vec2 t;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
        
        // Conversion from Az3D::Vertex
        static Vertex fromAz3D(const Az3D::Vertex& az3dVertex);
        static std::vector<Vertex> fromAz3D(const std::vector<Az3D::Vertex>& az3dVertices);
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    // Instance data structure for instanced rendering
    struct InstanceData {
        alignas(16) glm::mat4 modelMatrix;
        
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();
    };

    class Buffer {
    public:
        Buffer(const class VulkanDevice& device);
        ~Buffer();

        // Delete copy constructor and assignment operator
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        void createVertexBuffer(const std::vector<Vertex>& vertices);
        void createIndexBuffer(const std::vector<uint16_t>& indices);
        void createIndexBuffer(const std::vector<uint32_t>& indices);
        void createUniformBuffers(size_t count);
        
        // New methods for Az3D integration
        void loadMesh(const Az3D::Mesh& mesh);
        void createVertexBuffer(const Az3D::Mesh& mesh);
        void createInstanceBuffer(const std::vector<InstanceData>& instances);
        void updateInstanceBuffer(const std::vector<InstanceData>& instances);

        
        const VulkanDevice& vulkanDevice;
        
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        VkIndexType indexType = VK_INDEX_TYPE_UINT16;
        
        // Instance buffer for instanced rendering
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
        void* instanceBufferMapped = nullptr;
        uint32_t instanceCount = 0;
        
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        // Helper methods (now public for direct access)
        void createBuffer(  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);
    };
}
