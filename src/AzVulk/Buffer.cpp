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
        
        // Create vertex buffer for this mesh
        const auto& vertices = mesh.vertices;
        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        vulkanDevice.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    meshBuffer.vertexBuffer, meshBuffer.vertexBufferMemory);

        void* vertexData;
        vkMapMemory(vulkanDevice.device, meshBuffer.vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData);
        memcpy(vertexData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(vulkanDevice.device, meshBuffer.vertexBufferMemory);
        
        // Create index buffer for this mesh
        const auto& indices = mesh.indices;
        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

        vulkanDevice.createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    meshBuffer.indexBuffer, meshBuffer.indexBufferMemory);

        meshBuffer.indexCount = static_cast<uint32_t>(indices.size());
        meshBuffer.indexType = VK_INDEX_TYPE_UINT32;

        void* indexData;
        vkMapMemory(vulkanDevice.device, meshBuffer.indexBufferMemory, 0, indexBufferSize, 0, &indexData);
        memcpy(indexData, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(vulkanDevice.device, meshBuffer.indexBufferMemory);
        
        // Add to meshBuffers vector and return index
        meshBuffers.push_back(meshBuffer);
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