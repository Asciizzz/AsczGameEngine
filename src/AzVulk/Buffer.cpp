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

        dataTypeSize = other.dataTypeSize;
        resourceCount = other.resourceCount;
        totalDataSize = other.totalDataSize;

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

            dataTypeSize = other.dataTypeSize;
            resourceCount = other.resourceCount;
            totalDataSize = other.totalDataSize;

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
        const Device& vulkanDevice, size_t resourceCount, size_t dataTypeSize,
        VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
    ) {
        this->usageFlags = usageFlags;
        this->memoryFlags = memoryFlags;
        this->device = vulkanDevice.device;
        this->physicalDevice = vulkanDevice.physicalDevice;

        this->resourceCount = static_cast<uint32_t>(resourceCount);
        this->dataTypeSize = static_cast<VkDeviceSize>(dataTypeSize);
        this->totalDataSize = static_cast<VkDeviceSize>(dataTypeSize * resourceCount);

        cleanup();

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = totalDataSize;
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
        for (auto& bufferData : uniformBuffers) {
            bufferData.cleanup();
        }

        for (auto& bufferData : meshBuffers) {
            bufferData.cleanup(vulkanDevice.device);
        }

        for (auto& bufferData : materialBuffers) {
            bufferData.cleanup();
        }
    }

    void Buffer::createUniformBuffers(size_t count) {

        uniformBuffers.resize(count);

        for (size_t i = 0; i < count; ++i) {
            auto& bufferData = uniformBuffers[i];

            bufferData.createBuffer(
                vulkanDevice, 1, sizeof(GlobalUBO), BufferData::TransferSrc,
                BufferData::HostVisible | BufferData::HostCoherent
            );

            bufferData.mapData();
        }
    }

    void Buffer::creatematerialBuffers(const std::vector<Az3D::Material>& materials) {
        materialBuffers.resize(materials.size());

        for (size_t i = 0; i < materials.size(); ++i) {
            auto& bufferData = materialBuffers[i];

            bufferData.createBuffer(
                vulkanDevice, 1, sizeof(MaterialUBO), BufferData::Uniform,
                BufferData::HostVisible | BufferData::HostCoherent
            );

            MaterialUBO materialUBO(materials[i].prop1);
            bufferData.uploadData(&materialUBO);
        }
    }

    // New multi-mesh methods implementation
    size_t Buffer::createMeshBuffer(const Az3D::Mesh& mesh) {
        MeshBufferData meshBuffer;

        const auto& vertices = mesh.vertices;
        const auto& indices = mesh.indices;

        // Beta: Using the new BufferData struct
        meshBuffer.vertexBufferData.createBuffer(
            vulkanDevice, vertices.size(), sizeof(Az3D::Vertex),
            BufferData::Vertex, BufferData::HostVisible | BufferData::HostCoherent);
        meshBuffer.vertexBufferData.uploadData(vertices);

        meshBuffer.indexBufferData.createBuffer(
            vulkanDevice, indices.size(), sizeof(uint32_t),
            BufferData::Index, BufferData::HostVisible | BufferData::HostCoherent);
        meshBuffer.indexBufferData.uploadData(indices);

        // Add to meshBuffers vector and return index
        meshBuffers.push_back(std::move(meshBuffer));
        return meshBuffers.size() - 1;
    }


    void Buffer::createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances) {
        const auto& instanceIndices = meshData.instanceIndices;
        auto& meshBuffer = meshBuffers[meshIndex];

        auto& instanceBufferData = meshBuffer.instanceBufferData;

        instanceBufferData.createBuffer(
            vulkanDevice, instanceIndices.size(), sizeof(Az3D::InstanceVertexData),
            BufferData::Vertex, BufferData::HostVisible | BufferData::HostCoherent
        );

        instanceBufferData.mapData();

        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            static_cast<Az3D::InstanceVertexData*>(instanceBufferData.mapped)[i] = modelInstances[instanceIndices[i]].vertexData;
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
            static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferData.mapped)[bufferPos] = modelInstances[instanceIndex].vertexData;
        });

        // for (size_t instanceIndex : meshData.updateIndices) {
        //     size_t bufferPos = meshData.instanceToBufferPos.find(instanceIndex)->second;
        //     static_cast<Az3D::InstanceVertexData*>(meshBuffer.instanceBufferMapped)[bufferPos] = modelInstances[instanceIndex].vertexData;
        // }
    }



}