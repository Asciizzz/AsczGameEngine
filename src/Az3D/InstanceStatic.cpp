#include "Az3D/InstanceStatic.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace Az3D {

// Vulkan-specific methods for Model
VkVertexInputBindingDescription InstanceStatic::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 1;
    bindingDescription.stride = sizeof(InstanceStatic);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 6> InstanceStatic::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 6> attribs{};

    attribs[0] = {2, 1, VK_FORMAT_R32G32B32A32_SINT, offsetof(InstanceStatic, properties)};

    attribs[1] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, modelMatrix) + sizeof(glm::vec4) * 0};
    attribs[2] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, modelMatrix) + sizeof(glm::vec4) * 1};
    attribs[3] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, modelMatrix) + sizeof(glm::vec4) * 2};
    attribs[4] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, modelMatrix) + sizeof(glm::vec4) * 3};

    attribs[5] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, multColor)};

    return attribs;
}

// Add data
size_t InstanceStaticGroup::addInstance(const InstanceStatic& data) {
    datas.push_back(data);
    return datas.size() - 1;
}

void InstanceStaticGroup::initVkDevice(const AzVulk::Device* vkDevice) {
    bufferData.initVkDevice(vkDevice);
}

void InstanceStaticGroup::recreateBufferData() {
    if (!bufferData.vkDevice) return;

    bufferData.setProperties(
        datas.size() * sizeof(InstanceStatic), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    bufferData.createBuffer();
    bufferData.mappedData(datas.data());

    prevInstanceCount = datas.size();
}

void InstanceStaticGroup::updateBufferData() {
    if (!bufferData.vkDevice) return;

    if (prevInstanceCount != datas.size()) recreateBufferData();

    bufferData.mappedData(datas.data());
}

}
