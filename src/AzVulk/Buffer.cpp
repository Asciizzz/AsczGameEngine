#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>

#include <stb_image.h>

namespace AzVulk {

    // Move constructor
    BufferData::BufferData(BufferData&& other) noexcept {
        device = other.device;
        physicalDevice = other.physicalDevice;

        buffer = other.buffer;
        memory = other.memory;
        mapped = other.mapped;

        dataSize = other.dataSize;
        resourceCount = other.resourceCount;

        usageFlags = other.usageFlags;
        memoryFlags = other.memoryFlags;

        // Invalidate other's handles
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }

    // Move assignment
    BufferData& BufferData::operator=(BufferData&& other) noexcept {
        if (this != &other) {
            cleanup();
            device = other.device;
            physicalDevice = other.physicalDevice;
            
            buffer = other.buffer;
            memory = other.memory;
            mapped = other.mapped;

            dataSize = other.dataSize;
            resourceCount = other.resourceCount;

            usageFlags = other.usageFlags;
            memoryFlags = other.memoryFlags;

            // Invalidate other's handles
            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.mapped = nullptr;
        }
        return *this;
    }

    void BufferData::cleanup() {
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

    void BufferData::createBuffer(
        const Device& vulkanDevice, size_t dataTypeSize, size_t resourceCount,
        VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
    ) {
        this->usageFlags = usageFlags;
        this->memoryFlags = memoryFlags;
        this->device = vulkanDevice.device;
        this->physicalDevice = vulkanDevice.physicalDevice;

        this->dataSize = static_cast<VkDeviceSize>(dataTypeSize * resourceCount);
        this->resourceCount = static_cast<uint32_t>(resourceCount);

        cleanup();

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = dataSize;
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





    Buffer::Buffer(const Device& device) : vulkanDevice(device) {}

    Buffer::~Buffer() {
        VkDevice logicalDevice = vulkanDevice.device;

        // Cleanup multi-mesh buffers
        for (auto& meshBuffer : meshBuffers) {
            meshBuffer.cleanup(logicalDevice);
        }

        // Cleanup uniform buffers
        for (size_t i = 0; i < uniformBuffers.size(); ++i) {
            vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
        }

        // Cleanup material uniform buffers
        for (size_t i = 0; i < materialUniformBuffers.size(); ++i) {
            if (materialUniformBuffersMapped[i]) {
                vkUnmapMemory(logicalDevice, materialUniformBuffersMemory[i]);
            }
            vkDestroyBuffer(logicalDevice, materialUniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, materialUniformBuffersMemory[i], nullptr);
        }
    }

    void Buffer::createUniformBuffers(size_t count) {
        VkDeviceSize bufferSize = sizeof(GlobalUBO);

        uniformBuffers.resize(count);
        uniformBuffersMemory.resize(count);
        uniformBuffersMapped.resize(count);

        for (size_t i = 0; i < count; ++i) {
            vulkanDevice.createBuffer(
                bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                uniformBuffers[i], uniformBuffersMemory[i]);
            // Robust: unmap if already mapped
            if (uniformBuffersMapped[i]) {
                vkUnmapMemory(vulkanDevice.device, uniformBuffersMemory[i]);
                uniformBuffersMapped[i] = nullptr;
            }
            vkMapMemory(vulkanDevice.device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void Buffer::createMaterialUniformBuffers(const std::vector<Az3D::Material>& materials) {
        VkDeviceSize bufferSize = sizeof(MaterialUBO);

        materialUniformBuffers.resize(materials.size());
        materialUniformBuffersMemory.resize(materials.size());
        materialUniformBuffersMapped.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            vulkanDevice.createBuffer(
                bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                materialUniformBuffers[i], materialUniformBuffersMemory[i]);
            // Robust: unmap if already mapped
            if (materialUniformBuffersMapped[i]) {
                vkUnmapMemory(vulkanDevice.device, materialUniformBuffersMemory[i]);
                materialUniformBuffersMapped[i] = nullptr;
            }
            vkMapMemory(vulkanDevice.device, materialUniformBuffersMemory[i], 0, bufferSize, 0, &materialUniformBuffersMapped[i]);

            MaterialUBO materialUBO(materials[i].prop1);

            memcpy(materialUniformBuffersMapped[i], &materialUBO, sizeof(MaterialUBO));
        }
    }

    void Buffer::updateMaterialUniformBuffer(size_t materialIndex, const Az3D::Material& material) {
        if (materialIndex < materialUniformBuffersMapped.size() && materialUniformBuffersMapped[materialIndex]) {
            MaterialUBO materialUBO(material.prop1);
            memcpy(materialUniformBuffersMapped[materialIndex], &materialUBO, sizeof(MaterialUBO));
        }
    }

    VkBuffer Buffer::getMaterialUniformBuffer(size_t materialIndex) const {
        return materialUniformBuffers[materialIndex];
    }

    // New multi-mesh methods implementation
    size_t Buffer::loadMeshToBuffer(const Az3D::Mesh& mesh) {
        MeshBufferData meshBuffer;

        const auto& vertices = mesh.vertices;
        const auto& indices = mesh.indices;

        // Beta: Using the new BufferData struct
        meshBuffer.vertexBufferData.createBuffer(
            vulkanDevice, sizeof(Az3D::Vertex), vertices.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        meshBuffer.vertexBufferData.uploadData(vertices);

        meshBuffer.indexBufferData.createBuffer(
            vulkanDevice, sizeof(uint32_t), indices.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        meshBuffer.indexBufferData.uploadData(indices);

        // Add to meshBuffers vector and return index
        meshBuffers.push_back(std::move(meshBuffer));
        return meshBuffers.size() - 1;
    }


    void Buffer::createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances) {
        const auto& instanceIndices = meshData.instanceIndices;
        auto& meshBuffer = meshBuffers[meshIndex];
        VkDeviceSize bufferSize = sizeof(Az3D::InstanceVertexData) * instanceIndices.size();

        // Clean up existing buffer if it exists
        if (meshBuffer.instanceBuffer != VK_NULL_HANDLE) {
            if (meshBuffer.instanceBufferMapped) {
                vkUnmapMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory);
                meshBuffer.instanceBufferMapped = nullptr;
            }
            vkDestroyBuffer(vulkanDevice.device, meshBuffer.instanceBuffer, nullptr);
            vkFreeMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory, nullptr);
        }

        vulkanDevice.createBuffer(
            bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            meshBuffer.instanceBuffer, meshBuffer.instanceBufferMemory);

        // Map the buffer for updates
        vkMapMemory(vulkanDevice.device, meshBuffer.instanceBufferMemory, 0, bufferSize, 0, &meshBuffer.instanceBufferMapped);

        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[i] = modelInstances[instanceIndices[i]].vertexData;
        }

        // Update previous instance count directly in the mesh data
        meshData.prevInstanceCount = instanceIndices.size();
    }

    void Buffer::updateMeshInstanceBufferSelective( size_t meshIndex,
                                                    Az3D::MeshMappingData& meshData, 
                                                    const std::vector<Az3D::ModelInstance>& modelInstances) {
        auto& meshBuffer = meshBuffers[meshIndex];

        std::for_each(std::execution::par_unseq, meshData.updateIndices.begin(), meshData.updateIndices.end(), [&](size_t instanceIndex) {
            size_t bufferPos = meshData.instanceToBufferPos.find(instanceIndex)->second;
            static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[bufferPos] = modelInstances[instanceIndex].vertexData;
        });

        // for (size_t instanceIndex : meshData.updateIndices) {
        //     size_t bufferPos = meshData.instanceToBufferPos.find(instanceIndex)->second;
        //     static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[bufferPos] = modelInstances[instanceIndex].vertexData;
        // }
    }



}