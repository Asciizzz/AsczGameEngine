// GlobalUBOManager.cpp (simplified to only handle UBO)

#include "Az3D/GlobalUBO.hpp"
#include "Az3D/Camera.hpp"

#include <stdexcept>
#include <chrono>

using namespace AzVulk;

namespace Az3D {

GlobalUBOManager::GlobalUBOManager(const Device* vkDevice, uint32_t MAX_FRAMES_IN_FLIGHT)
: vkDevice(vkDevice), MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT)
{
    createBufferDatas();
    initDescriptorSets();
    createDescriptorSets();
}

void GlobalUBOManager::createBufferDatas() {
    bufferDatas.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& bufferData = bufferDatas[i];
        bufferData.initVkDevice(vkDevice);

        bufferData.setProperties(
            sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        bufferData.createBuffer();

        bufferData.mapMemory();
    }
}

void GlobalUBOManager::initDescriptorSets() {
    using namespace AzVulk;

    dynamicDescriptor.init(vkDevice->device);
    dynamicDescriptor.createLayout({
        // Global UBO only
        DynamicDescriptor::fastBinding(
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        )
    });

    dynamicDescriptor.createPool({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT}
    }, MAX_FRAMES_IN_FLIGHT);
}

void GlobalUBOManager::createDescriptorSets() {
    using namespace AzVulk;

    // Free old sets
    for (auto& set : dynamicDescriptor.sets) {
        if (set != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(vkDevice->device, dynamicDescriptor.pool, 1, &set);
            set = VK_NULL_HANDLE;
        }
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, dynamicDescriptor.setLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = dynamicDescriptor.pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    dynamicDescriptor.sets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vkDevice->device, &allocInfo, dynamicDescriptor.sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate global descriptor sets");
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = bufferDatas[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GlobalUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = dynamicDescriptor.sets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(vkDevice->device, 1, &write, 0, nullptr);
    }
}

VkDescriptorSet GlobalUBOManager::getDescriptorSet(uint32_t frameIndex) {
    return dynamicDescriptor.sets[frameIndex];
}

float deltaDay = 1.0f / 86400.0f;

void GlobalUBOManager::updateUBO(const Camera& camera) {
    ubo.proj = camera.projectionMatrix;
    ubo.view = camera.viewMatrix;

    float elapsedSeconds =
        std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    float timeOfDay = fmod(elapsedSeconds * deltaDay, 1.0f);
    ubo.prop1 = glm::vec4(timeOfDay, 0.0f, 0.0f, 0.0f);

    ubo.cameraPos     = glm::vec4(camera.pos, glm::radians(camera.fov));
    ubo.cameraForward = glm::vec4(camera.forward, camera.aspectRatio);
    ubo.cameraRight   = glm::vec4(camera.right, camera.nearPlane);
    ubo.cameraUp      = glm::vec4(camera.up, camera.farPlane);
}

}
