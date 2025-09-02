#include "Az3D/StaticInstance.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

using namespace Az3D;

// Vulkan-specific methods for Model
VkVertexInputBindingDescription StaticInstance::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 1;
    bindingDescription.stride = sizeof(StaticInstance);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> StaticInstance::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribs(4);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_UINT, offsetof(StaticInstance, properties)};

    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, trformT_S)};
    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, trformR)};

    attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, multColor)};

    return attribs;
}


size_t StaticInstanceGroup::addInstance(const StaticInstance& data) {
    datas.push_back(data);
    return datas.size() - 1;
}

void StaticInstanceGroup::initVkDevice(const AzVulk::Device* vkDevice) {
    bufferData.initVkDevice(vkDevice);
}

void StaticInstanceGroup::recreateBufferData() {
    if (!bufferData.vkDevice) return;

    bufferData.setProperties(
        datas.size() * sizeof(StaticInstance), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    bufferData.createBuffer();
    bufferData.mapAndCopy(datas.data());

    prevInstanceCount = datas.size();
}

void StaticInstanceGroup::updateBufferData() {
    if (!bufferData.vkDevice) return;

    if (prevInstanceCount != datas.size()) recreateBufferData();

    bufferData.mapAndCopy(datas.data());
}
