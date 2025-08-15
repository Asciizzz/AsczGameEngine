#include "Az3D/GlobalUBO.hpp"
#include "Az3D/Camera.hpp"

namespace Az3D {

    GlobalUBOManager::GlobalUBOManager(
        VkDevice device, VkPhysicalDevice physicalDevice, uint32_t MAX_FRAMES_IN_FLIGHT,
        // Add additional global component as needed
        VkSampler depthSampler, VkImageView depthSamplerView

    ) : device(device), physicalDevice(physicalDevice), MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT),
        // Add additional global components as needed (1)
        depthSampler(depthSampler), depthSamplerView(depthSamplerView)

    {
        createBufferDatas();
        initDescriptorSets();
        createDescriptorSets();
    }

    void GlobalUBOManager::createBufferDatas() {
        bufferDatas.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto& bufferData = bufferDatas[i];
            bufferData.initVulkan(device, physicalDevice);

            bufferData.createBuffer(
                1, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );

            bufferData.mappedData();
        }
    }

    void GlobalUBOManager::initDescriptorSets() {
        using namespace AzVulk;

        // Create layout
        dynamicDescriptor.init(device);
        dynamicDescriptor.createSetLayout({
            // Global UBO
            DynamicDescriptor::fastBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
            // Depth sampler
            DynamicDescriptor::fastBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            // ...
        });

        // Create pool
        dynamicDescriptor.createPool({
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT}
            // ...
        }, MAX_FRAMES_IN_FLIGHT);
    }

    void GlobalUBOManager::createDescriptorSets() {
        using namespace AzVulk;

        // Re"set"
        for (auto& set : dynamicDescriptor.sets) {
            vkFreeDescriptorSets(device, dynamicDescriptor.pool, 1, &set);
            set = VK_NULL_HANDLE;
        }

        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, dynamicDescriptor.setLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = dynamicDescriptor.pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        dynamicDescriptor.sets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, dynamicDescriptor.sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate global descriptor sets");
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = bufferDatas[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GlobalUBO);

            VkDescriptorImageInfo depthInfo{};
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.imageView = depthSamplerView;
            depthInfo.sampler = depthSampler;

            std::array<VkWriteDescriptorSet, 2> writes{};

            // UBO
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = dynamicDescriptor.sets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo = &bufferInfo;

            // Depth sampler
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = dynamicDescriptor.sets[i];
            writes[1].dstBinding = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &depthInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    VkDescriptorSet GlobalUBOManager::getDescriptorSet(uint32_t frameIndex) {
        return dynamicDescriptor.sets[frameIndex];
    }

    // Functionalities
    void GlobalUBOManager::updateUBO(const Camera& camera) {
        ubo.proj = camera.projectionMatrix;
        ubo.view = camera.viewMatrix;
        ubo.cameraPos = glm::vec4(camera.pos, glm::radians(camera.fov));
        ubo.cameraForward = glm::vec4(camera.forward, camera.aspectRatio);
        ubo.cameraRight = glm::vec4(camera.right, 0.0f);
        ubo.cameraUp = glm::vec4(camera.up, 0.0f);
        ubo.nearFar = glm::vec4(camera.nearPlane, camera.farPlane, 0.0f, 0.0f);
    }

    void GlobalUBOManager::resizeWindow(VkSampler newDepthSampler, VkImageView newDepthSamplerView) {
        depthSampler = newDepthSampler;
        depthSamplerView = newDepthSamplerView;

        // Remake descriptor sets
        createDescriptorSets();
    }

}