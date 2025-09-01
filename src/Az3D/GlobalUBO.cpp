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
    bufferData.initVkDevice(vkDevice);

    bufferData.setProperties(
        sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    bufferData.createBuffer();

    bufferData.mapMemory();
}

void GlbUBOManager::createDescLayout() {
    descLayout.create(vkDevice->lDevice, {
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
    });
}

void GlbUBOManager::createDescPool() {
    descPool.create(vkDevice->lDevice, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    }, 1);
}

void GlbUBOManager::createDescSet() {
    VkDevice lDevice = vkDevice->lDevice;

    descSet.allocate(lDevice, descPool.pool, descLayout.layout, 1);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = bufferData.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(GlobalUBO);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet.get();
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &write, 0, nullptr);
}

float deltaDay = 1.0f / 86400.0f;

void GlbUBOManager::updateUBO(const Camera& camera) {
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

    memcpy(bufferData.mapped, &ubo, sizeof(ubo));
}
