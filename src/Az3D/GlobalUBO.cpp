#include "Az3D/GlobalUBO.hpp"
#include "Az3D/Camera.hpp"

#include <stdexcept>
#include <chrono>

using namespace AzVulk;

using namespace Az3D;

GlbUBOManager::GlbUBOManager(const Device* vkDevice)
: vkDevice(vkDevice)
{
    createBufferData();

    createDescLayout();
    createDescPool();
    createDescSet();
}

void GlbUBOManager::createBufferData() {
    bufferData.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        bufferData[i].initVkDevice(vkDevice);

        bufferData[i].setProperties(
            sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        bufferData[i].createBuffer();
        bufferData[i].mapMemory();
    }
}

void GlbUBOManager::createDescLayout() {
    descLayout.init(vkDevice->lDevice);

    descLayout.create({
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
    });
}

void GlbUBOManager::createDescPool() {
    descPool.init(vkDevice->lDevice);

    descPool.create({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT}
    }, MAX_FRAMES_IN_FLIGHT);
}

void GlbUBOManager::createDescSet() {
    descSets.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        descSets[i].init(vkDevice->lDevice);
        descSets[i].allocate(descPool.pool, descLayout.layout, 1);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = bufferData[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GlobalUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSets[i].get();
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(descSets[i].lDevice, 1, &write, 0, nullptr);
    }
}

float deltaDay = 1.0f / 86400.0f;

void GlbUBOManager::updateUBO(const Camera& camera, uint32_t frameIndex) {
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

    memcpy(bufferData[frameIndex].mapped, &ubo, sizeof(ubo));
}
