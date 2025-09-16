#pragma once

#include "Tiny3D/TinyModel.hpp"

#include "AzVulk/DataBuffer.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/ImageVK.hpp"

namespace Az3D {

struct MaterialVK {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albTexIndex, nrmlTexIndex, unused, unused>
};

struct LightVK {
    glm::vec4 position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // xyz = position, w = light type (0=directional, 1=point, 2=spot)
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // rgb = color, a = intensity
    glm::vec4 direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); // xyz = direction (for directional/spot), w = range
    glm::vec4 params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // x = inner cone angle, y = outer cone angle, z = attenuation, w = unused
};

// Index pointing to certain Vulkan elements
struct ModelPtr {
    std::vector<size_t> submeshVK_indices;

    std::vector<uint32_t> materialVK_indices;
    size_t skeletonIndex = -1;

    // Some helpers
    std::vector<uint32_t> submesh_indexCounts; // Cached index counts for each submesh
    size_t submeshCount() const { return submeshVK_indices.size(); }
};

struct SubmeshVK {
    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32; // Default to uint32
};

struct ModelVK {
    std::vector<SubmeshVK> submeshVKs;

    AzVulk::DataBuffer matBuffer; // Big buffer for all materials of THIS model
    VkDescriptorSet matDescSet; // Descriptor set for the material buffer
};



// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* deviceVK);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return matDescSet->getLayout(); }
    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }

    VkDescriptorSetLayout getTexDescLayout() const { return texDescSet->getLayout(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return skeleDescSets->getLayout(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return skeleDescSets->get(index); }

    VkDescriptorSetLayout getLightDescLayout() const { return lightDescSet->getLayout(); }
    VkDescriptorSet getLightDescSet() const { return lightDescSet->get(); }

    size_t addModel(const TinyModel& model) {
        models.push_back(model);
        return models.size() - 1;
    }

    // Light management
    size_t addLight(const LightVK& light) {
        lightVKs.push_back(light);
        lightsDirty = true;
        return lightVKs.size() - 1;
    }

    void updateLight(size_t index, const LightVK& light) {
        if (index < lightVKs.size()) {
            lightVKs[index] = light;
            lightsDirty = true;
        }
    }

    void removeLight(size_t index) {
        if (index < lightVKs.size()) {
            lightVKs.erase(lightVKs.begin() + index);
            lightsDirty = true;
        }
    }

    uint32_t getLightCount() const { return static_cast<uint32_t>(lightVKs.size()); }

    void updateLightBuffer(); // Update buffer when lights change

    void uploadAllToGPU();

    void createComponentVKsFromModels();

// private:
    AzVulk::Device* deviceVK;

    std::vector<TinyModel>            models;
    std::vector<ModelPtr>              modelVKs; // Contain indices to vulkan resources
    std::vector<MaterialVK>           materialVKs; // Very different from TinyMaterial
    UniquePtrVec<SubmeshVK>           submeshVKs;
    std::vector<AzVulk::ImageVK> textures;

    size_t addSubmeshVK(const TinySubmesh& submesh);
    VkBuffer getSubmeshVertexBuffer(size_t submeshVK_index) const;
    VkBuffer getSubmeshIndexBuffer(size_t submeshVK_index) const;
    VkIndexType getSubmeshIndexType(size_t submeshVK_index) const;

    SharedPtrVec<TinySkeleton>        skeletons;
    UniquePtrVec<AzVulk::DataBuffer>  skeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescSets>       skeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

    UniquePtr<AzVulk::DataBuffer>     matBuffer;
    UniquePtr<AzVulk::DescSets>       matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    std::vector<VkSampler>            samplers;
    void createTextureSamplers();

    std::vector<uint32_t>             texSamplerIndices; // Per-texture sampler index (maps to samplers array)
    UniquePtr<AzVulk::DataBuffer>     textSampIdxBuffer; // Buffer containing sampler indices for each texture
    void createTexSampIdxBuffer(); // Create buffer for texture sampler indices

    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Light system
    std::vector<LightVK>              lightVKs; // Dynamic light data
    UniquePtr<AzVulk::DataBuffer>     lightBuffer; // Host-writable buffer for dynamic updates
    UniquePtr<AzVulk::DescSets>       lightDescSet;
    bool lightsDirty = false;
    void createLightBuffer();
    void createLightDescSet();

    // Useful methods
    AzVulk::ImageVK createTexture(const TinyTexture& texture);
};

} // namespace Az3D
