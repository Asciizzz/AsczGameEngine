#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace AzVulk {

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

        // Cleanup material uniform buffers
        for (size_t i = 0; i < materialUniformBuffers.size(); i++) {
            if (materialUniformBuffersMapped[i]) {
                vkUnmapMemory(logicalDevice, materialUniformBuffersMemory[i]);
            }
            vkDestroyBuffer(logicalDevice, materialUniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, materialUniformBuffersMemory[i], nullptr);
        }

        // Cleanup legacy buffers
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

    void Buffer::createVertexBuffer(const std::vector<Az3D::Vertex>& vertices) {
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
        VkDeviceSize bufferSize = sizeof(GlobalUBO);

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

    void Buffer::createMaterialUniformBuffers(const std::vector<Az3D::Material>& materials) {
        VkDeviceSize bufferSize = sizeof(MaterialUBO);

        materialUniformBuffers.resize(materials.size());
        materialUniformBuffersMemory.resize(materials.size());
        materialUniformBuffersMapped.resize(materials.size());

        for (size_t i = 0; i < materials.size(); i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                        materialUniformBuffers[i], materialUniformBuffersMemory[i]);
            
            vkMapMemory(vulkanDevice.device, materialUniformBuffersMemory[i], 0, bufferSize, 0, &materialUniformBuffersMapped[i]);
            
            // Initialize material data
            MaterialUBO materialUBO{};
            materialUBO.multColor = materials[i].multColor;
            memcpy(materialUniformBuffersMapped[i], &materialUBO, sizeof(MaterialUBO));
        }
    }

    void Buffer::updateMaterialUniformBuffer(size_t materialIndex, const Az3D::Material& material) {
        if (materialIndex < materialUniformBuffersMapped.size() && materialUniformBuffersMapped[materialIndex]) {
            MaterialUBO materialUBO{};
            materialUBO.multColor = material.multColor;
            memcpy(materialUniformBuffersMapped[materialIndex], &materialUBO, sizeof(MaterialUBO));
        }
    }

    VkBuffer Buffer::getMaterialUniformBuffer(size_t materialIndex) const {
        if (materialIndex < materialUniformBuffers.size()) {
            return materialUniformBuffers[materialIndex];
        }
        return VK_NULL_HANDLE;
    }

    // New Az3D integration methods
    void Buffer::loadMesh(const Az3D::Mesh& mesh) {
        // Use Az3D vertices directly
        createVertexBuffer(mesh.vertices);
        createIndexBuffer(mesh.indices);
    }

    void Buffer::createVertexBuffer(const Az3D::Mesh& mesh) {
        createVertexBuffer(mesh.vertices);
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

    void Buffer::createInstanceBuffer(const std::vector<Az3D::ModelInstance>& instances) {
        VkDeviceSize bufferSize = sizeof(Az3D::InstanceVertexData) * instances.size();
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
        
        // Copy only GPU vertex data
        for (size_t i = 0; i < instances.size(); ++i) {
            static_cast<Az3D::InstanceVertexData*>(instanceBufferMapped)[i] = instances[i].vertexData;
        }
    }

    void Buffer::updateInstanceBuffer(const std::vector<Az3D::ModelInstance>& instances) {
        if (instanceBufferMapped && instances.size() <= instanceCount) {
            // Copy only GPU vertex data
            for (size_t i = 0; i < instances.size(); ++i) {
                static_cast<Az3D::InstanceVertexData*>(instanceBufferMapped)[i] = instances[i].vertexData;
            }
        }
    }

    // New multi-mesh methods implementation
    size_t Buffer::loadMeshToBuffer(const Az3D::Mesh& mesh) {
        MeshBufferData meshBuffer;
        
        // Use Az3D vertices directly
        const auto& vertices = mesh.vertices;
        
        // Create vertex buffer for this mesh
        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    meshBuffer.vertexBuffer, meshBuffer.vertexBufferMemory);

        void* vertexData;
        vkMapMemory(vulkanDevice.device, meshBuffer.vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData);
        memcpy(vertexData, vertices.data(), (size_t)vertexBufferSize);
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

    void Buffer::createInstanceBufferForMesh(size_t meshIndex, const std::vector<Az3D::ModelInstance>& instances) {
        if (meshIndex >= meshBuffers.size()) {
            throw std::runtime_error("Invalid mesh index for instance buffer creation!");
        }
        
        auto& meshBuffer = meshBuffers[meshIndex];
        VkDeviceSize bufferSize = sizeof(Az3D::InstanceVertexData) * instances.size();
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
        
        // Copy only GPU vertex data
        for (size_t i = 0; i < instances.size(); ++i) {
            static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[i] = instances[i].vertexData;
        }
    }

    void Buffer::updateInstanceBufferForMesh(size_t meshIndex, const std::vector<Az3D::ModelInstance>& instances) {
        if (meshIndex >= meshBuffers.size()) {
            return; // Silently fail for invalid mesh index
        }

        auto& meshBuffer = meshBuffers[meshIndex];
        if (meshBuffer.instanceBufferMapped && instances.size() <= meshBuffer.instanceCount) {
            // Copy only GPU vertex data
            for (size_t i = 0; i < instances.size(); ++i) {
                static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[i] = instances[i].vertexData;
            }
        }
    }
}
