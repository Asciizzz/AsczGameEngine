#include "tinyEngine/tinyGlobal.hpp"

#include "tinyData/tinyCamera.hpp"

#include <stdexcept>
#include <chrono>

using namespace tinyVk;

void tinyGlobal::vkCreate(const tinyVk::Device* deviceVk) {
    // get the true aligned size (for dynamic UBO offsets)
    alignedSize = deviceVk->alignSize(sizeof(tinyGlobal::UBO));

    dataBuffer
        .setDataSize(alignedSize * maxFramesInFlight)
        .setUsageFlags(BufferUsage::Uniform)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVk)
        .mapMemory();

    VkDevice device = deviceVk->device;

    descLayout.create(device, { {0, DescType::UniformBufferDynamic, 1, ShaderStage::VertexAndFragment, nullptr} });
    descPool.create(device, { {DescType::UniformBufferDynamic, 1} }, 1);
    descSet.allocate(device, descPool, descLayout);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = dataBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(tinyGlobal::UBO);

    DescWrite()
        .setDstSet(descSet)
        .setType(DescType::UniformBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({bufferInfo})
        .updateDescSets(device);
}


void tinyGlobal::update(const tinyCamera& camera, uint32_t frameIndex) {
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

    // During copy, you need to use the correct size and offset
    size_t offset = alignedSize * frameIndex;
    dataBuffer.copyData(&ubo, sizeof(ubo), offset);
}
