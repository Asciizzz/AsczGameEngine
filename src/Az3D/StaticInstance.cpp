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
    std::vector<VkVertexInputAttributeDescription> attribs(3);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, trformT_S)};
    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, trformR)};

    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StaticInstance, multColor)};

    return attribs;
}


size_t StaticInstanceGroup::addInstance(const StaticInstance& data) {
    datas.push_back(data);
    return datas.size() - 1;
}

void StaticInstanceGroup::initVkDevice(const AzVulk::Device* vkDevice) {
    initVkDevice(vkDevice->lDevice, vkDevice->pDevice);
}
void StaticInstanceGroup::initVkDevice(VkDevice lDevice, VkPhysicalDevice pDevice) {
    this->lDevice = lDevice;
    this->pDevice = pDevice;
}

void StaticInstanceGroup::recreateDataBuffer() {
    if (dataBuffer.lDevice == VK_NULL_HANDLE) return;

    dataBuffer
        .setProperties(
            datas.size() * sizeof(StaticInstance), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        )
        .createBuffer(lDevice, pDevice)
        .mapAndCopy(datas.data());

    prevInstanceCount = datas.size();
}

void StaticInstanceGroup::updateDataBuffer() {
    if (dataBuffer.lDevice == VK_NULL_HANDLE) return;

    if (prevInstanceCount != datas.size()) recreateDataBuffer();

    dataBuffer.mapAndCopy(datas.data());
}
