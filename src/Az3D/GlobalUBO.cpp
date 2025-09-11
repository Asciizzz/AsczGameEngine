#include "Az3D/GlobalUBO.hpp"
#include "Az3D/Camera.hpp"

#include <stdexcept>
#include <chrono>

using namespace AzVulk;

using namespace Az3D;

GlbUBOManager::GlbUBOManager(const Device* vkDevice)
: vkDevice(vkDevice)
{
    createDataBuffer();

    createDescSets();
}

void GlbUBOManager::createDataBuffer() {
    dataBuffer.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        dataBuffer[i]
            .setProperties(
                sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            )
            .createBuffer(vkDevice)
            .mapMemory();
    }
}

void GlbUBOManager::createDescSets() {
    descSets.init(vkDevice->lDevice);

    descSets.createLayout({
        DescSets::LayoutBind{0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
    });

    descSets.createPool({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT}
    }, MAX_FRAMES_IN_FLIGHT);

    descSets.allocate(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = dataBuffer[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GlobalUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSets.get(i);
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(vkDevice->lDevice, 1, &write, 0, nullptr);
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

    memcpy(dataBuffer[frameIndex].mapped, &ubo, sizeof(ubo));
}
