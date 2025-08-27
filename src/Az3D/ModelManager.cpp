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

std::array<VkVertexInputAttributeDescription, 6> ModelData::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions{};

    // Properties (location 2)
    attributeDescriptions[0].binding = 1;
    attributeDescriptions[0].location = 2;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SINT;
    attributeDescriptions[0].offset = offsetof(ModelData, properties);

    // Model matrix is 4x4, so we need 4 attribute locations (3, 4, 5, 6)
    // Each vec4 takes one attribute location
    for (int i = 1; i < 5; ++i) {
        attributeDescriptions[i].binding = 1;
        attributeDescriptions[i].location = 2 + i;
        attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[i].offset = offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * (i - 1);
    }

    // Instance color multiplier vec4 (location 7) - directly after modelMatrix in Data
    attributeDescriptions[5].binding = 1;
    attributeDescriptions[5].location = 7;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(ModelData, multColor);

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
    bufferData.mappedData(datas.data());

    prevInstanceCount = datas.size();
}

void ModelMappingData::updateBufferData() {
    if (!bufferData.vkDevice) return;

    if (prevInstanceCount != datas.size()) recreateBufferData();

    bufferData.mappedData(datas.data());
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
