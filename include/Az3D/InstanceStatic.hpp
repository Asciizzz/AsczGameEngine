#pragma once

#include <string>
#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "Helpers/AzPair.hpp"

namespace Az3D {

// Dynamic, per-frame object data
struct InstanceStatic {
    InstanceStatic() = default;

    glm::uvec4 properties = glm::uvec4(0); // <materialIndex>, <indicator>, <empty>, <empty>
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec4 multColor = glm::vec4(1.0f);

    // Vulkan-specific methods for vertex input
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions();
};


/*
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
    std::vector<InstanceStatic> datas;
    size_t addData(const InstanceStatic& data);

    AzVulk::BufferData bufferData;
    void initVkDevice(const AzVulk::Device* lDevice);
    void recreateBufferData();
    void updateBufferData();
};

// Model group for separate renderer
struct ModelGroup {
    // Mesh - Material (in that order)
    using Hash = AzPair<1'000'000, 1'000'000>;

    std::string name = "Default";

    const AzVulk::Device* vkDevice = nullptr;

    ModelGroup() = default;
    ModelGroup(const std::string& name, const AzVulk::Device* lDevice) : name(name), vkDevice(lDevice) {}
    void init(const std::string& name, const AzVulk::Device* lDevice) { this->name = name; vkDevice = lDevice; }

    ModelGroup(const ModelGroup&) = delete;
    ModelGroup& operator=(const ModelGroup&) = delete;
    ModelGroup(ModelGroup&&) noexcept = default;
    ModelGroup& operator=(ModelGroup&&) noexcept = default;


    // Hash Model -> data
    UnorderedMap<size_t, ModelMappingData> modelMapping;

    void addInstance(size_t meshIndex, size_t materialIndex, const InstanceStatic& instanceData);
};
*/

struct InstanceStaticGroup {
    InstanceStaticGroup() = default;

    InstanceStaticGroup(const InstanceStaticGroup&) = delete;
    InstanceStaticGroup& operator=(const InstanceStaticGroup&) = delete;
    InstanceStaticGroup(InstanceStaticGroup&&) noexcept = default;
    InstanceStaticGroup& operator=(InstanceStaticGroup&&) noexcept = default;

    size_t prevInstanceCount = 0;
    std::vector<InstanceStatic> datas;
    size_t addInstance(const InstanceStatic& data);

    AzVulk::BufferData bufferData;
    void initVkDevice(const AzVulk::Device* vkDevice);
    void recreateBufferData();
    void updateBufferData();

    uint32_t meshIndex = SIZE_MAX; // We only need this value
};

} // namespace Az3D
