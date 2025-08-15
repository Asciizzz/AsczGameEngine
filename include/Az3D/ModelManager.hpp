#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

    // Dynamic, per-frame object data
    struct ModelInstance {
        ModelInstance() = default;

        struct Data3D {
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
        ModelMappingData(ModelMappingData&& other) noexcept;
        ModelMappingData& operator=(ModelMappingData&& other) noexcept;

        size_t prevInstanceCount = 0;
        std::vector<ModelInstance::Data3D> datas;
        size_t addData(const ModelInstance::Data3D& data);

        bool vulkanFlag = false;
        AzVulk::BufferData bufferData;
        void initVulkanDevice(VkDevice device, VkPhysicalDevice physicalDevice);
        void recreateBufferData();
        void updateBufferData();
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
            it->second.addData(instance.data);
        }
        void addInstance(size_t meshIndex, size_t materialIndex, const ModelInstance::Data3D& instanceData) {
            size_t modelEncode = ModelPair::encode(meshIndex, materialIndex);

            auto [it, inserted] = modelMapping.try_emplace(modelEncode);
            if (inserted) { // New entry
                it->second.initVulkanDevice(device, physicalDevice);
            }
            // Add to existing entry
            it->second.addData(instanceData);
        }
    };

} // namespace Az3D
