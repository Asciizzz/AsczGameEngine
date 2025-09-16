#include "Az3D/RigDemo.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Az3D;

void RigDemo::init(const AzVulk::Device* deviceVK, const TinyModel& model, size_t modelIndex) {
    using namespace AzVulk;

    this->model = MakeShared<TinyModel>(model);
    this->modelIndex = modelIndex;

    playback.setSkeleton(this->model->skeleton);  // Use skeleton from the copied model

    // Vulkan stuff, very cringe, yet very cool
    
    VkDeviceSize bufferSize = sizeof(glm::mat4) * model.skeleton.names.size();

    finalPoseBuffer
        .setDataSize(bufferSize)
        .setUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        .setMemPropFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        .createBuffer(deviceVK)
        .mapMemory();

    updateBuffer();

    descSet.init(deviceVK->lDevice);
    descSet.createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    });

    descSet.createOwnPool({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

    descSet.allocate(1);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = finalPoseBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet.get();
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(deviceVK->lDevice, 1, &write, 0, nullptr);
}

void RigDemo::update(float dTime) {
    updatePlayback(dTime);
    updateBuffer();
}

void RigDemo::updateBuffer() {
    finalPoseBuffer.copyData(playback.boneMatrices.data());
}
void RigDemo::updatePlayback(float dTime) {
    playback.update(dTime);
}

void RigDemo::playAnimation(size_t animIndex, bool loop, float speed, float transitionTime) {
    if (!model || model->animations.empty()) {
        return;
    }

    // Use the specified animation index
    if (animIndex >= model->animations.size()) {
        std::cerr << "Invalid animation index: " << animIndex << "\n";
        return;
    }

    const auto& animation = model->animations[animIndex];
    playback.playAnimation(animation, loop, speed, transitionTime);
}