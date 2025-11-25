#include "tinyEngine/tinyGlobal.hpp"

#include "tinyCamera.hpp"

#include <stdexcept>
#include <chrono>

using namespace tinyVk;

void tinyGlobal::vkCreate(const tinyVk::Device* dvk) {
    // get the true aligned size (for dynamic UBO offsets)
    alignedSize = dvk->alignSizeUBO(sizeof(tinyGlobal::UBO));

    dataBuffer
        .setDataSize(alignedSize * maxFramesInFlight)
        .setUsageFlags(BufferUsage::Uniform)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk)
        .mapMemory();

    VkDevice device = dvk->device;

    descSLayout.create(device, { {0, DescType::UniformBufferDynamic, 1, ShaderStage::VertexAndFragment, nullptr} });
    descPool.create(device, { {DescType::UniformBufferDynamic, 1} }, 1);
    descSet.allocate(device, descPool, descSLayout);

    DescWrite()
        .setDstSet(descSet)
        .setType(DescType::UniformBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            dataBuffer,
            0,
            alignedSize
        } })
        .updateDescSets(device);
}


void tinyGlobal::update(const tinyCamera& camera, uint32_t frameIndex) {

    ubo.proj = camera.projectionMatrix;
    ubo.view = camera.viewMatrix;

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto now = std::chrono::high_resolution_clock::now();
    float elapsedSeconds = std::chrono::duration<float>(now - startTime).count();

    static constexpr float deltaDay = 1.0f / 86400.0f; // 1 day = 86400 seconds
    float timeSpeed = 60.0f; // 1 real second = 1 in-game minute
    float timeOfDay = fmod(elapsedSeconds * deltaDay * timeSpeed, 1.0f);
    ubo.prop1 = glm::vec4(timeOfDay, 0.0f, 0.0f, 0.0f);

    // During copy, you need to use the correct size and offset
    size_t offset = alignedSize * frameIndex;
    dataBuffer.copyData(&ubo, sizeof(ubo), offset);
}
