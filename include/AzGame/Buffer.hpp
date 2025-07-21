#pragma once

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace AzGame {
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
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
        void createUniformBuffers(size_t count);

        VkBuffer getVertexBuffer() const { return vertexBuffer; }
        VkBuffer getIndexBuffer() const { return indexBuffer; }
        const std::vector<VkBuffer>& getUniformBuffers() const { return uniformBuffers; }
        const std::vector<void*>& getUniformBuffersMapped() const { return uniformBuffersMapped; }

    private:
        const VulkanDevice& vulkanDevice;
        
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                         VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);
    };
}
