#include "Az3D/ModelManager.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace Az3D {

    // Vulkan-specific methods for Model
    VkVertexInputBindingDescription ModelData::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1; // Binding 1 for instance data
        bindingDescription.stride = sizeof(ModelData); // Only GPU data
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 5> ModelData::getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        // Model matrix is 4x4, so we need 4 attribute locations (3, 4, 5, 6)
        // Each vec4 takes one attribute location
        for (int i = 0; i < 4; ++i) {
            attributeDescriptions[i].binding = 1;
            attributeDescriptions[i].location = 3 + i; // Locations 3, 4, 5, 6
            attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[i].offset = offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * i;
        }

        // Instance color multiplier vec4 (location 7) - directly after modelMatrix in Data
        attributeDescriptions[4].binding = 1;
        attributeDescriptions[4].location = 7;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(ModelData, multColor);

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
    size_t ModelMappingData::addData(const ModelData& data) {
        datas.push_back(data);
        return datas.size() - 1;
    }

    void ModelMappingData::initVulkanDevice(const AzVulk::Device* device) {
        bufferData.initVkDevice(device);
    }

    void ModelMappingData::recreateBufferData() {
        if (!bufferData.vkDevice) return;

        bufferData.setProperties(
            datas.size() * sizeof(ModelData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        bufferData.createBuffer();
        bufferData.mappedData(datas);

        prevInstanceCount = datas.size();
    }

    void ModelMappingData::updateBufferData() {
        if (!bufferData.vkDevice) return;

        if (prevInstanceCount != datas.size()) recreateBufferData();

        bufferData.mappedData(datas);
    }


    void ModelGroup::addInstance(size_t meshIndex, size_t materialIndex, const ModelData& instanceData) {
        if (!vkDevice) return;

        size_t modelEncode = Hash::encode(meshIndex, materialIndex);

        auto [it, inserted] = modelMapping.try_emplace(modelEncode);
        if (inserted) { // New entry
            it->second.initVulkanDevice(vkDevice);
        }
        // Add to existing entry
        it->second.addData(instanceData);
    }
}
