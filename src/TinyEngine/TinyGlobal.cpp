#include "TinyEngine/TinyGlobal.hpp"

#include "TinyData/TinyCamera.hpp"

#include <stdexcept>
#include <chrono>

using namespace TinyVK;

void TinyGlobal::createDataBuffer(const TinyVK::Device* deviceVK) {
    dataBuffer.resize(maxFramesInFlight);
    
    for (int i = 0; i < maxFramesInFlight; ++i) {
        dataBuffer[i]
            .setDataSize(sizeof(TinyGlobal::UBO))
            .setUsageFlags(BufferUsage::Uniform)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(deviceVK)
            .mapMemory();
    }
}

void TinyGlobal::createDescResources(const TinyVK::Device* deviceVK) {
    VkDevice lDevice = deviceVK->lDevice;

    descLayout.create(lDevice,
        {{0, DescType::UniformBuffer, 1, ShaderStage::VertexAndFragment, nullptr}
    });

    descPool.create(lDevice, {
        {DescType::UniformBuffer, maxFramesInFlight}
    }, maxFramesInFlight);

    for (int i = 0; i < maxFramesInFlight; ++i) {
        DescSet descSet;
        descSet.allocate(lDevice, descPool, descLayout);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = dataBuffer[i].get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(TinyGlobal::UBO);

        DescWrite()
            .setDstSet(descSet)
            .setDescType(DescType::UniformBuffer)
            .setDescCount(1)
            .setBufferInfo({bufferInfo})
            .updateDescSets(lDevice);

        descSets.push_back(std::move(descSet));
    }
}

void TinyGlobal::createVkResources(const TinyVK::Device* deviceVK) {
    createDataBuffer(deviceVK);
    createDescResources(deviceVK);
}


void TinyGlobal::update(const TinyCamera& camera, uint32_t frameIndex) {
    static constexpr float deltaDay = 1.0f / 86400.0f;

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
