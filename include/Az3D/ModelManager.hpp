#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

    // Dynamic, per-frame object data
    struct ModelInstance {
        ModelInstance() = default;

        struct Data {
            alignas(16) glm::mat4 modelMatrix = glm::mat4(1.0f);
            alignas(16) glm::vec4 multColor = glm::vec4(1.0f);
        } data;

        size_t meshIndex = SIZE_MAX;
        size_t materialIndex = SIZE_MAX;

        // Vulkan-specific methods for vertex input
        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
    };


    struct ModelMappingData {
        ModelMappingData() = default;
        ~ModelMappingData() { bufferData.cleanup(); }

        // Delete copy constructor and assignment operator
        ModelMappingData(const ModelMappingData&) = delete;
        ModelMappingData& operator=(const ModelMappingData&) = delete;

        // Move semantics
        ModelMappingData(ModelMappingData&& other) noexcept
            : datas(std::move(other.datas)),
            bufferData(std::move(other.bufferData)) {}

        ModelMappingData& operator=(ModelMappingData&& other) noexcept {
            datas = std::move(other.datas);
            bufferData = std::move(other.bufferData);
            return *this;
        }

        size_t prevInstanceCount = 0;
        std::vector<ModelInstance::Data> datas;
        size_t addInstance(const ModelInstance& instance) {
            datas.push_back(instance.data);
            return datas.size() - 1;
        }
        size_t addInstance(const ModelInstance::Data& instanceData) {
            datas.push_back(instanceData);
            return datas.size() - 1;
        }

        bool vulkanFlag = false;
        AzVulk::BufferData bufferData;
        void initVulkanDevice(VkDevice device, VkPhysicalDevice physicalDevice) {
            bufferData.initVulkanDevice(device, physicalDevice);
            vulkanFlag = true;
        }

        void recreateBufferData() {
            bufferData.createBuffer( // Already contain safeguards
                datas.size(), sizeof(Az3D::ModelInstance::Data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            bufferData.mappedData(datas);

            prevInstanceCount = datas.size();
        }

        void updateBufferData() {
            if (!vulkanFlag) return;

            if (prevInstanceCount != datas.size()) recreateBufferData();

            bufferData.mappedData(datas);
        }

    };

    // Model group for separate renderer
    struct ModelGroup {
        // No chance in hell we passing 1 million meshes/materials
        using ModelPair = AzPair<1'000'000, 1'000'000>;

        ModelGroup() = default;
        ModelGroup(const std::string& name, VkDevice device, VkPhysicalDevice physicalDevice)
            : name(name), device(device), physicalDevice(physicalDevice) {}
        void initVulkanDevice(VkDevice device, VkPhysicalDevice physicalDevice) {
            this->device = device;
            this->physicalDevice = physicalDevice;
        }

        ModelGroup(const ModelGroup&) = delete;
        ModelGroup& operator=(const ModelGroup&) = delete;
        ModelGroup(ModelGroup&&) noexcept = default;
        ModelGroup& operator=(ModelGroup&&) noexcept = default;

        std::string name = "Default";
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        // Hash Model -> data
        UnorderedMap<size_t, ModelMappingData> modelMapping;

        void addInstance(const ModelInstance& instance) {
            size_t modelEncode = ModelPair::encode(instance.meshIndex, instance.materialIndex);
            
            auto [it, inserted] = modelMapping.try_emplace(modelEncode);
            if (inserted) { // New entry
                it->second.initVulkanDevice(device, physicalDevice);
            }
            // Add to existing entry
            it->second.addInstance(instance);
        }
        void addInstance(size_t meshIndex, size_t materialIndex, const ModelInstance::Data& instanceData) {
            size_t modelEncode = ModelPair::encode(meshIndex, materialIndex);

            auto [it, inserted] = modelMapping.try_emplace(modelEncode);
            if (inserted) { // New entry
                it->second.initVulkanDevice(device, physicalDevice);
            }
            // Add to existing entry
            it->second.addInstance(instanceData);
        }
    };

} // namespace Az3D
