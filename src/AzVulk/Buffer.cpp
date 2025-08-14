#include "AzVulk/Buffer.hpp"
#include "AzVulk/Device.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <execution>

namespace AzVulk {

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
        meshBuffer.vertexBufferData.mapData(vertices);

        meshBuffer.indexBufferData.createBuffer(
            vulkanDevice, sizeof(uint32_t), indices.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        meshBuffer.indexBufferData.mapData(indices);

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