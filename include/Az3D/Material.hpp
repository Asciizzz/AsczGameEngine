#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

struct Material {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albedo>, <normal>, <metallic>, <unsure>

    Material() = default;

    void setShadingParams(bool shading, int toonLevel, float normalBlend, float discardThreshold) {
        shadingParams.x = shading ? 1.0f : 0.0f;
        shadingParams.y = static_cast<float>(toonLevel);
        shadingParams.z = normalBlend;
        shadingParams.w = discardThreshold;
    }

    void setAlbedoTextureIndex(int index) {
        texIndices.x = index;
    }

    void setNormalTextureIndex(int index) {
        texIndices.y = index;
    }
};


// MaterialGroup - manages materials using index-based access
class MaterialGroup {
public:
    MaterialGroup(const AzVulk::Device* vkDevice);

    MaterialGroup(const MaterialGroup&) = delete;
    MaterialGroup& operator=(const MaterialGroup&) = delete;

    const AzVulk::Device* vkDevice;

    size_t addMaterial(const Material& material);

    std::vector<Material> materials;

    AzVulk::BufferData bufferData;
    void createGPUBufferData();

    AzVulk::DescSets descSet;
    void createDescSet(const VkDescriptorPool pool, const VkDescriptorSetLayout layout);
    VkDescriptorSet getDescSet() const { return descSet.get(); }


    void uploadToGPU();
};

} // namespace Az3D
