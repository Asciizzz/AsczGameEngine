#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>
#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

    // Dynamic, per-frame object data
    struct Model {
        Model() = default;

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
        std::vector<Model::Data3D> datas;
        size_t addData(const Model::Data3D& data);

        AzVulk::BufferData bufferData;
        void initVulkanDevice(const AzVulk::Device* device);
        void recreateBufferData();
        void updateBufferData();
    };

    // Model group for separate renderer
    struct ModelGroup {
        // No chance in hell we passing 1 million meshes/materials
        using ModelPair = AzPair<1'000'000, 1'000'000>;

        std::string name = "Default";

        const AzVulk::Device* vkDevice = nullptr;

        ModelGroup() = default;
        ModelGroup(const std::string& name, const AzVulk::Device* device) : name(name), vkDevice(device) {}
        void init(const std::string& name, const AzVulk::Device* device) { this->name = name; vkDevice = device; }

        ModelGroup(const ModelGroup&) = delete;
        ModelGroup& operator=(const ModelGroup&) = delete;
        ModelGroup(ModelGroup&&) noexcept = default;
        ModelGroup& operator=(ModelGroup&&) noexcept = default;


        // Hash Model -> data
        UnorderedMap<size_t, ModelMappingData> modelMapping;

        void addInstance(const Model& instance);
        void addInstance(size_t meshIndex, size_t materialIndex, const Model::Data3D& instanceData);
    };

} // namespace Az3D
