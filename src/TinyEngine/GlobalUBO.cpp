#include "TinyEngine/GlobalUBO.hpp"

#include "TinyData/TinyCamera.hpp"

#include <stdexcept>
#include <chrono>

using namespace AzVulk;

using namespace TinyEngine;

GlbUBOManager::GlbUBOManager(const DeviceVK* deviceVK, uint32_t maxFramesInFlight)
: deviceVK(deviceVK), maxFramesInFlight(maxFramesInFlight) {
    createDataBuffer();

    createDescSets();
}

void GlbUBOManager::createDataBuffer() {
    dataBuffer.resize(maxFramesInFlight);
    
    for (int i = 0; i < maxFramesInFlight; ++i) {
        dataBuffer[i]
            .setDataSize(sizeof(GlobalUBO))
            .setUsageFlags(BufferUsage::Uniform)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(deviceVK)
            .mapMemory();
    }
}

void GlbUBOManager::createDescSets() {
    VkDevice lDevice = deviceVK->lDevice;

    descLayout = MakeUnique<DescLayout>();
    descLayout->create(lDevice,
        {{0, DescType::UniformBuffer, 1, ShaderStage::VertexAndFragment, nullptr}
    });

    descPool = MakeUnique<DescPool>();
    descPool->create(lDevice, {
        {DescType::UniformBuffer, maxFramesInFlight}
    }, maxFramesInFlight);

    for (int i = 0; i < maxFramesInFlight; ++i) {
        UniquePtr<DescSet> descSet = MakeUnique<DescSet>();
        descSet->allocate(lDevice, *descPool, *descLayout);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = dataBuffer[i].get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GlobalUBO);

        DescWrite()
            .setDstSet(*descSet)
            .setDescType(DescType::UniformBuffer)
            .setDescCount(1)
            .setBufferInfo({bufferInfo})
            .updateDescSet(lDevice);

        descSets.push_back(std::move(descSet));
    }
}

float deltaDay = 1.0f / 86400.0f;

void GlbUBOManager::updateUBO(const TinyCamera& camera, uint32_t frameIndex) {
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

    // memcpy(dataBuffer[frameIndex].mapped, &ubo, sizeof(ubo));
    dataBuffer[frameIndex].copyData(&ubo);
}
