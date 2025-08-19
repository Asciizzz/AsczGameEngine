#include "Az3D/ModelManager.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace Az3D {

    // Vulkan-specific methods for Model
    VkVertexInputBindingDescription Model::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1; // Binding 1 for instance data
        bindingDescription.stride = sizeof(Data3D); // Only GPU data
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 5> Model::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        // Model matrix is 4x4, so we need 4 attribute locations (3, 4, 5, 6)
        // Each vec4 takes one attribute location
        for (int i = 0; i < 4; ++i) {
            attributeDescriptions[i].binding = 1;
            attributeDescriptions[i].location = 3 + i; // Locations 3, 4, 5, 6
            attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[i].offset = offsetof(Data3D, modelMatrix) + sizeof(glm::vec4) * i;
        }

        // Instance color multiplier vec4 (location 7) - directly after modelMatrix in Data
        attributeDescriptions[4].binding = 1;
        attributeDescriptions[4].location = 7;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Data3D, multColor);

        return attributeDescriptions;
    }


// Move semantics
    ModelMappingData::ModelMappingData(ModelMappingData&& other) noexcept
        : datas(std::move(other.datas)),
        bufferData(std::move(other.bufferData)) {}

    ModelMappingData& ModelMappingData::operator=(ModelMappingData&& other) noexcept {
        datas = std::move(other.datas);
        bufferData = std::move(other.bufferData);
        return *this;
    }

// Add data
    size_t ModelMappingData::addData(const Model::Data3D& data) {
        datas.push_back(data);
        return datas.size() - 1;
    }

    void ModelMappingData::initVulkanDevice(const AzVulk::Device* device) {
        bufferData.initVulkanDevice(device);
    }

    void ModelMappingData::recreateBufferData() {
        if (!bufferData.vkDevice) return;

        bufferData.createBuffer( // Already contain safeguards
            datas.size(), sizeof(Az3D::Model::Data3D), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        bufferData.mappedData(datas);

        prevInstanceCount = datas.size();
    }

    void ModelMappingData::updateBufferData() {
        if (!bufferData.vkDevice) return;

        if (prevInstanceCount != datas.size()) recreateBufferData();

        bufferData.mappedData(datas);
    }



    void ModelGroup::addInstance(const Model& instance) {
        if (!vkDevice) return;

        size_t modelEncode = ModelPair::encode(instance.meshIndex, instance.materialIndex);
        
        auto [it, inserted] = modelMapping.try_emplace(modelEncode);
        if (inserted) { // New entry
            it->second.initVulkanDevice(vkDevice);
        }
        // Add to existing entry
        it->second.addData(instance.data);
    }

    void ModelGroup::addInstance(size_t meshIndex, size_t materialIndex, const Model::Data3D& instanceData) {
        if (!vkDevice) return;

        size_t modelEncode = ModelPair::encode(meshIndex, materialIndex);

        auto [it, inserted] = modelMapping.try_emplace(modelEncode);
        if (inserted) { // New entry
            it->second.initVulkanDevice(vkDevice);
        }
        // Add to existing entry
        it->second.addData(instanceData);
    }
}
