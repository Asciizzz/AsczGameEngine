#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace AzVulk {
    VkVertexInputBindingDescription Vertex::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, p);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, n);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, t);

        return attributeDescriptions;
    }

    // Conversion methods from Az3D::Vertex to AzVulk::Vertex
    Vertex Vertex::fromAz3D(const Az3D::Vertex& az3dVertex) {
        return Vertex{
            az3dVertex.pos,
            az3dVertex.nrml,
            az3dVertex.txtr
        };
    }

    std::vector<Vertex> Vertex::fromAz3D(const std::vector<Az3D::Vertex>& az3dVertices) {
        std::vector<Vertex> result;
        result.reserve(az3dVertices.size());
        
        for (const auto& az3dVertex : az3dVertices) {
            result.push_back(fromAz3D(az3dVertex));
        }
        
        return result;
    }

    Buffer::Buffer(const Device& device) : vulkanDevice(device) {}

    Buffer::~Buffer() {
        VkDevice logicalDevice = vulkanDevice.device;

        // Cleanup multi-mesh buffers
        for (auto& meshBuffer : meshBuffers) {
            meshBuffer.cleanup(logicalDevice);
        }

        // Cleanup uniform buffers
        for (size_t i = 0; i < uniformBuffers.size(); i++) {
            vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
        }

        // Cleanup legacy single-mesh buffers
        if (instanceBuffer != VK_NULL_HANDLE) {
            if (instanceBufferMapped) {
                vkUnmapMemory(logicalDevice, instanceBufferMemory);
            }
            vkDestroyBuffer(logicalDevice, instanceBuffer, nullptr);
            vkFreeMemory(logicalDevice, instanceBufferMemory, nullptr);
        }

        if (indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
            vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
        }

        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
            vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
        }
    }

    void Buffer::createVertexBuffer(const std::vector<Vertex>& vertices) {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // For simplicity, create buffer directly in host-visible memory
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    vertexBuffer, vertexBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice.device, vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanDevice.device, vertexBufferMemory);
    }

    void Buffer::createIndexBuffer(const std::vector<uint16_t>& indices) {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        indexCount = static_cast<uint32_t>(indices.size());
        indexType = VK_INDEX_TYPE_UINT16;

        // For simplicity, create buffer directly in host-visible memory
        createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    indexBuffer, indexBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice.device, indexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanDevice.device, indexBufferMemory);
    }

    void Buffer::createIndexBuffer(const std::vector<uint32_t>& indices) {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        indexCount = static_cast<uint32_t>(indices.size());
        indexType = VK_INDEX_TYPE_UINT32;

        // For simplicity, create buffer directly in host-visible memory
        createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    indexBuffer, indexBufferMemory);

        void* data;
        vkMapMemory(vulkanDevice.device, indexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanDevice.device, indexBufferMemory);
    }

    void Buffer::createUniformBuffers(size_t count) {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(count);
        uniformBuffersMemory.resize(count);
        uniformBuffersMapped.resize(count);

        for (size_t i = 0; i < count; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                        uniformBuffers[i], uniformBuffersMemory[i]);
            
            vkMapMemory(vulkanDevice.device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    // New Az3D integration methods
    void Buffer::loadMesh(const Az3D::Mesh& mesh) {
        // Convert Az3D vertices to AzVulk vertices and create buffers
        auto vulkVertices = Vertex::fromAz3D(mesh.vertices);
        createVertexBuffer(vulkVertices);
        createIndexBuffer(mesh.indices);
    }

    void Buffer::createVertexBuffer(const Az3D::Mesh& mesh) {
        auto vulkVertices = Vertex::fromAz3D(mesh.vertices);
        createVertexBuffer(vulkVertices);
    }

    void Buffer::createBuffer(  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                                VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(vulkanDevice.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vulkanDevice.device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = vulkanDevice.findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(vulkanDevice.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(vulkanDevice.device, buffer, bufferMemory, 0);
    }

    void Buffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(vulkanDevice.device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(vulkanDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanDevice.graphicsQueue);

        vkFreeCommandBuffers(vulkanDevice.device, commandPool, 1, &commandBuffer);
    }

    // InstanceData methods for instanced rendering
    VkVertexInputBindingDescription InstanceData::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1; // Binding 1 for instance data
        bindingDescription.stride = sizeof(InstanceData);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 4> InstanceData::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        // Model matrix is 4x4, so we need 4 attribute locations (3, 4, 5, 6)
        // Each vec4 takes one attribute location
        for (int i = 0; i < 4; i++) {
            attributeDescriptions[i].binding = 1;
            attributeDescriptions[i].location = 3 + i; // Locations 3, 4, 5, 6
            attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[i].offset = offsetof(InstanceData, modelMatrix) + sizeof(glm::vec4) * i;
        }

        return attributeDescriptions;
    }

    void Buffer::createInstanceBuffer(const std::vector<InstanceData>& instances) {
        VkDeviceSize bufferSize = sizeof(InstanceData) * instances.size();
        instanceCount = static_cast<uint32_t>(instances.size());

        // Clean up existing buffer if it exists
        if (instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice.device, instanceBuffer, nullptr);
            vkFreeMemory(vulkanDevice.device, instanceBufferMemory, nullptr);
        }

        createBuffer(bufferSize, 
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    instanceBuffer, instanceBufferMemory);

        // Map the buffer for updates
        vkMapMemory(vulkanDevice.device, instanceBufferMemory, 0, bufferSize, 0, &instanceBufferMapped);
        
        // Copy initial data
        memcpy(instanceBufferMapped, instances.data(), bufferSize);
    }

    void Buffer::updateInstanceBuffer(const std::vector<InstanceData>& instances) {
        if (instanceBufferMapped && instances.size() <= instanceCount) {
            VkDeviceSize bufferSize = sizeof(InstanceData) * instances.size();
            memcpy(instanceBufferMapped, instances.data(), bufferSize);
        }
    }

    // New multi-mesh methods implementation
    size_t Buffer::loadMeshToBuffer(const Az3D::Mesh& mesh) {
        MeshBufferData meshBuffer;
        
        // Convert Az3D vertices to AzVulk vertices
        auto vulkVertices = Vertex::fromAz3D(mesh.vertices);
        
        // Create vertex buffer for this mesh
        VkDeviceSize vertexBufferSize = sizeof(vulkVertices[0]) * vulkVertices.size();
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    meshBuffer.vertexBuffer, meshBuffer.vertexBufferMemory);

        void* vertexData;
        vkMapMemory(vulkanDevice.device, meshBuffer.vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData);
        memcpy(vertexData, vulkVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(vulkanDevice.device, meshBuffer.vertexBufferMemory);
        
        // Create index buffer for this mesh
        const auto& indices = mesh.indices;
        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        meshBuffer.indexCount = static_cast<uint32_t>(indices.size());
        meshBuffer.indexType = VK_INDEX_TYPE_UINT32;

        createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    meshBuffer.indexBuffer, meshBuffer.indexBufferMemory);

        void* indexData;
        vkMapMemory(vulkanDevice.device, meshBuffer.indexBufferMemory, 0, indexBufferSize, 0, &indexData);
        memcpy(indexData, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(vulkanDevice.device, meshBuffer.indexBufferMemory);
        
        // Add to meshBuffers vector and return index
        meshBuffers.push_back(meshBuffer);
        return meshBuffers.size() - 1;
    }

    void Buffer::createInstanceBufferForMesh(size_t meshIndex, const std::vector<InstanceData>& instances) {
        if (meshIndex >= meshBuffers.size()) {
            throw std::runtime_error("Invalid mesh index for instance buffer creation!");
        }
        
        auto& meshBuffer = meshBuffers[meshIndex];
        VkDeviceSize bufferSize = sizeof(InstanceData) * instances.size();
        meshBuffer.instanceCount = static_cast<uint32_t>(instances.size());

        // Clean up existing buffer if it exists
        if (meshBuffer.instanceBuffer != VK_NULL_HANDLE) {
            if (meshBuffer.instanceBufferMapped) {
                vkUnmapMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory);
            }
            vkDestroyBuffer(vulkanDevice.device, meshBuffer.instanceBuffer, nullptr);
            vkFreeMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory, nullptr);
        }

        createBuffer(bufferSize, 
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    meshBuffer.instanceBuffer, meshBuffer.instanceBufferMemory);

        // Map the buffer for updates
        vkMapMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory, 0, bufferSize, 0, &meshBuffer.instanceBufferMapped);
        
        // Copy initial data
        memcpy(meshBuffer.instanceBufferMapped, instances.data(), bufferSize);
    }

    void Buffer::updateInstanceBufferForMesh(size_t meshIndex, const std::vector<InstanceData>& instances) {
        if (meshIndex >= meshBuffers.size()) {
            return; // Silently fail for invalid mesh index
        }
        
        auto& meshBuffer = meshBuffers[meshIndex];
        if (meshBuffer.instanceBufferMapped && instances.size() <= meshBuffer.instanceCount) {
            VkDeviceSize bufferSize = sizeof(InstanceData) * instances.size();
            memcpy(meshBuffer.instanceBufferMapped, instances.data(), bufferSize);
        }
    }
}
