#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include "Az3D/Az3D.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>


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
        size_t resourceCount, size_t dataTypeSize,
        VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryFlags
    ) {
        if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("BufferData: Vulkan device not initialized");
        }

        this->usageFlags = usageFlags;
        this->memoryFlags = memoryFlags;

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

    void BufferData::recreateBuffer(size_t resourceCount) {
        this->resourceCount = static_cast<uint32_t>(resourceCount);
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
        for (auto& bufferData : uniformBufferDatas)  bufferData.cleanup();
        for (auto& bufferData : instanceBufferDatas)     bufferData.cleanup();
    }

    void Buffer::createUniformBuffers(size_t count) {

        uniformBufferDatas.resize(count);

        for (size_t i = 0; i < count; ++i) {
            auto& bufferData = uniformBufferDatas[i];
            bufferData.initVulkan(vulkanDevice.device, vulkanDevice.physicalDevice);

            bufferData.createBuffer(
                1, sizeof(GlobalUBO), BufferData::TransferSrc,
                BufferData::HostVisible | BufferData::HostCoherent
            );

            bufferData.mappedData();
        }
    }


    void Buffer::createMeshInstanceBuffer(size_t meshIndex, Az3D::MeshMappingData& meshData, const std::vector<Az3D::ModelInstance>& modelInstances) {
        const auto& instanceIndices = meshData.instanceIndices;

        auto& instanceBufferData = instanceBufferDatas[meshIndex];

        instanceBufferData.initVulkan(vulkanDevice.device, vulkanDevice.physicalDevice);
        instanceBufferData.createBuffer(
            instanceIndices.size(), sizeof(Az3D::InstanceVertexData),
            BufferData::Vertex, BufferData::HostVisible | BufferData::HostCoherent
        );

        instanceBufferData.mappedData();

        for (size_t i = 0; i < instanceIndices.size(); ++i) {
            // static_cast<Az3D::InstanceVertexData*>(instanceBufferData.mapped)[i] = modelInstances[instanceIndices[i]].vertexData;
            instanceBufferData.updateMapped(i, modelInstances[instanceIndices[i]].vertexData);
        }

        // Update previous instance count directly in the mesh data
        meshData.prevInstanceCount = instanceIndices.size();
    }

    void Buffer::updateMeshInstanceBufferSelective( size_t meshIndex,
                                                    Az3D::MeshMappingData& meshData, 
                                                    const std::vector<Az3D::ModelInstance>& modelInstances) {
        auto& instanceBufferData = instanceBufferDatas[meshIndex];

        std::for_each(std::execution::par_unseq, meshData.updateIndices.begin(), meshData.updateIndices.end(), [&](size_t instanceIndex) {
            size_t bufferPos = meshData.instanceToBufferPos.find(instanceIndex)->second;
            instanceBufferData.updateMapped(bufferPos, modelInstances[instanceIndex].vertexData);
        });
    }



}