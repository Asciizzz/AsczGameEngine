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
    std::array<VkVertexInputAttributeDescription, 6> attribs{};

    attribs[0] = {2, 1, VK_FORMAT_R32G32B32A32_SINT, offsetof(ModelData, properties)};

    attribs[1] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * 0};
    attribs[2] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * 1};
    attribs[3] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * 2};
    attribs[4] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelData, modelMatrix) + sizeof(glm::vec4) * 3};

    attribs[5] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ModelData, multColor)};

    return attribs;
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
