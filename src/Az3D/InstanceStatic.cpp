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

std::vector<VkVertexInputAttributeDescription> InstanceStatic::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribs(4);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_UINT, offsetof(InstanceStatic, properties)};

    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, trformT_S)};
    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, trformR)};

    attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceStatic, multColor)};

    return attribs;
}


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
